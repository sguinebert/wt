/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include "Wt/Utils.h"
#include "Wt/WApplication.h"
#include "Wt/WCombinedLocalizedStrings.h"
#include "Wt/WContainerWidget.h"
#include "Wt/WException.h"
#include "Wt/WFormWidget.h"
#ifndef WT_TARGET_JAVA
#include "Wt/WIOService.h"
#endif
#include "Wt/WResource.h"
#include "Wt/WServer.h"
#include "Wt/WTimerWidget.h"
#include "Wt/Http/Request.h"

#include "CgiParser.h"
#include "Wt/Configuration.h"
#include "DomElement.h"
#include "Wt/WebController.h"
#include "WebRequest.h"
#include "WebSession.h"
#include "WebSocketMessage.h"
#include "WebUtils.h"

#include <boost/algorithm/string.hpp>
#ifndef _MSC_VER
#include <unistd.h>
#endif

#ifdef WT_WIN32
#include <process.h>
#endif

#ifdef WT_TARGET_JAVA
#define RETHROW(e) throw e
#else
#define RETHROW(e) throw
#endif

namespace {
  #ifdef WT_TARGET_JAVA
  static Wt::Http::UploadedFile* uf;
  #endif

  bool isAbsoluteUrl(const std::string& url) {
    return url.find(":") != std::string::npos;
  }

  std::string host(const std::string& url) {
    std::size_t pos = 0;
    for (unsigned i = 0; i < 3; ++i) { 
      pos = url.find('/', pos);
      if (pos == std::string::npos)
	return url;
      else
	++pos;
    }
    return url.substr(0, pos - 1);
  }

  inline std::string str(const char *v) {
    return v ? std::string(v) : std::string();
  }

  inline std::string str(std::string_view v) {
    return std::string(v);
  }

  inline bool isEqual(const char *s1, const char *s2) {
#ifdef WT_TARGET_JAVA
    if (s1 == 0) {
      return s2 == 0;
    } else {
      return std::string(s1) == s2;
    }
#else
    return strcmp(s1, s2) == 0;
#endif
  }
}

namespace Wt {

LOGGER("Wt");

#ifdef WT_TARGET_JAVA
boost::thread_specific_ptr<WebSession::Handler> WebSession::threadHandler_;
#else // WT_TARGET_JAVA
#ifdef WT_THREADED
static thread_local WebSession::Handler * threadHandler_ = nullptr;
#else // !WT_THREADED
static WebSession::Handler * threadHandler_ = nullptr;
#endif // WT_THREADED
#endif // WT_TARGET_JAVA

WebSession::WebSession(WebController *controller,
                       const std::string& sessionId,
                       EntryPointType type,
                       const std::string& favicon,
                       Wt::http::context *context,
                       WEnvironment *env)
  : type_(type),
    favicon_(favicon),
    state_(State::JustCreated),
    sessionId_(sessionId),
    sessionIdChanged_(false),
    sessionIdCookieChanged_(false),
    sessionIdInUrl_(false),
    controller_(controller),
    renderer_(*this),
    asyncResponse_(nullptr),
    webSocket_(nullptr),
    bootStyleResponse_(nullptr),
    canWriteWebSocket_(false),
    webSocketConnected_(false),
    pollRequestsIgnored_(0),
    progressiveBoot_(false),
    deferredRequest_(nullptr),
    //deferredResponse_(nullptr),
    deferCount_(0),
#ifdef WT_TARGET_JAVA
    recursiveEvent_(mutex_.newCondition()),
    recursiveEventDone_(mutex_.newCondition()),
    newRecursiveEvent_(nullptr),
    updatesPendingEvent_(mutex_.newCondition()),
#else
    newRecursiveEvent_(nullptr),
#endif
    updatesPending_(false),
    triggerUpdate_(false),
    embeddedEnv_(this),
    app_(nullptr),
    debug_(controller_->configuration().debug()),
    recursiveEventHandler_(nullptr)
{
  env_ = env ? env : &embeddedEnv_;

  // Update the URL scheme so we can set the session cookie correctly (with secure for https)
  if (context) {
      Configuration& conf = controller_->configuration();

      if (conf.behindReverseProxy() || conf.isTrustedProxy(context->req().hostname()))
      {
        std::string forwardedProto = str(context->getHeader("X-Forwarded-Proto"));
        if (!forwardedProto.empty()) {
            if (auto i = forwardedProto.rfind(','); i == std::string::npos)
            {
                env_->urlScheme_ = forwardedProto;
            }
            else
                env_->urlScheme_ = forwardedProto.substr(i+1);
        }
      }
      else {
        env_->urlScheme_ = context->req().scheme();
      }
  }
  env_->updateUrlScheme(context);

  /*
   * Obtain the applicationName_ as soon as possible for log().
   */
  if (context)
      applicationUrl_ = context->path(); //->scriptName();
  else
    applicationUrl_ = "/";

  deploymentPath_ = applicationUrl_;

  std::string::size_type slashpos = deploymentPath_.rfind('/');
  if (slashpos != std::string::npos) {
    basePath_ = deploymentPath_.substr(0, slashpos + 1);
    applicationName_ = deploymentPath_.substr(slashpos + 1);
  } else { // ?
    basePath_ = "";
    applicationName_ = applicationUrl_; 
  }

#ifndef WT_TARGET_JAVA
  LOG_INFO("session created (#sessions = {})", (controller_->sessionCount() + 1));

  expire_ = Time() + 60*1000;
#endif // WT_TARGET_JAVA

  if (controller_->configuration().sessionIdCookie())
  {
    sessionIdCookie_ = WRandom::generateId();
    sessionIdCookieChanged_ = true;
    renderer().setCookie("Wt" + sessionIdCookie_, "1", Wt::WDateTime(), "", "", env_->urlScheme() == "https");
  }
}

void WebSession::setApplication(WApplication *app)
{
  app_ = app;
}

void WebSession::deferRendering()
{
  if (!deferredRequest_) {
    Handler *handler = WebSession::Handler::instance();
    //deferredRequest_ = handler->request();
    deferredRequest_ = handler->context();
    handler->setRequest(nullptr);
  }

  ++deferCount_;
}

void WebSession::resumeRendering()
{
  if (--deferCount_ == 0) {
    Handler *handler = WebSession::Handler::instance();
    handler->setRequest(deferredRequest_);
    deferredRequest_ = nullptr;
    //deferredResponse_ = nullptr;
  }
}

void WebSession::setTriggerUpdate(bool update)
{
  triggerUpdate_ = update;
}

#ifndef WT_TARGET_JAVA
WLogger& WebSession::logInstance() const
{
    return controller_->server()->logger();
}

WLogEntry WebSession::log(const std::string& type) const
{
  if (controller_->server()->customLogger()) {
    return WLogEntry(*controller_->server()->customLogger(), type);
  }

  WLogEntry e = controller_->server()->logger().entry(type);

#ifndef WT_TARGET_JAVA
  e << WLogger::timestamp << WLogger::sep << getpid() << WLogger::sep
    << '[' << deploymentPath_ << ' ' << sessionId()
    << ']' << WLogger::sep << '[' << type << ']' << WLogger::sep;
#endif // WT_TARGET_JAVA

  return e;
}
#endif // WT_TARGET_JAVA

WebSession::~WebSession()
{
  /*
   * From here on, we cannot create a shared_ptr to this session. Therefore,
   * app_ uses a weak_ptr to this session for which lock() returns an empty
   * shared pointer.
   */
  state_ = State::Dead;

#ifndef WT_TARGET_JAVA
  Handler handler(this);

  if (app_)
  {
    //    app_->notify
    //      (WEvent(WEvent::Impl
    //	      (&handler, std::bind(&WApplication::finalize, app_))));
    try
    {
        app_->finalize();
        if (handler.context())
            render(handler);
    }
    catch (std::exception& e)
    {
        LOG_ERROR("Exception in WApplication::notify(): {}", e.what());

#ifdef WT_TARGET_JAVA
        e.printStackTrace();
#endif // WT_TARGET_JAVA
    }
    catch (...)
    {
        LOG_ERROR("Exception in WApplication::notify()");
    }
  }

  delete app_;
  app_ = nullptr;
#endif // WT_TARGET_JAVA

  if (asyncResponse_) {
    asyncResponse_->flush();
    asyncResponse_ = nullptr;
  }

  if (webSocket_) {
    webSocket_->flush();
    webSocket_ = nullptr;
  }

  if (deferredRequest_) {
    deferredRequest_->flush();
    deferredRequest_ = nullptr;
  }    

#ifdef WT_BOOST_THREADS
  updatesPendingEvent_.notify_one();
#endif // WT_BOOST_THREADS

  flushBootStyleResponse();

  controller_->configuration().registerSessionId(sessionId_, std::string());
  controller_->sessionDeleted();

#ifndef WT_TARGET_JAVA
  LOG_INFO("session destroyed (#sessions = {})", controller_->sessionCount());
#endif // WT_TARGET_JAVA

}

#ifdef WT_TARGET_JAVA
void WebSession::destruct()
{
  if (asyncResponse_) {
    asyncResponse_->flush();
    asyncResponse_ = nullptr;
  }

  if (deferredResponse_) {
    deferredResponse_->flush();
    deferredResponse_ = nullptr;
  }    

  mutex_.lock();
  updatesPendingEvent_.notify_one();
  mutex_.unlock();

  flushBootStyleResponse();
}
#endif // WT_TARGET_JAVA

std::string WebSession::docType() const
{
  const bool xhtml = env_->contentType() == HtmlContentType::XHTML1;

  if (xhtml)
    /*
     * This would be what we want, but it is too strict (does not
     * validate iframe's and target attribute for links):
     
     "\"-//W3C//DTD XHTML 1.1 plus MathML 2.0 plus SVG 1.1//EN\" "
     "\"http://www.w3.org/2002/04/xhtml-math-svg/xhtml-math-svg.dtd\">"
     * so instead we use transitional xhtml -- it will fail to
     * validate properly when we have svg !
     */ 
    return "<!DOCTYPE html PUBLIC "
      "\"-//W3C//DTD XHTML 1.0 Transitional//EN\" "
      "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">";
  else
    return
#ifdef HTML4_DOCTYPE
      "<!DOCTYPE html PUBLIC "
      "\"-//W3C//DTD HTML 4.01 Transitional//EN\" "
      "\"http://www.w3.org/TR/html4/loose.dtd\">";
#else
      "<!DOCTYPE html>"; // HTML5 hoeray
#endif
}

std::string_view clientAddress(http::request& request, const Wt::Configuration &conf)
{
  //std::string remoteAddr = str(envValue("REMOTE_ADDR"));
  auto remoteAddr = request.envValue("REMOTE_ADDR");
  if (conf.behindReverseProxy()) {
    // Old, deprecated behavior
    auto clientIp = request.get("Client-IP");

    std::vector<std::string_view> ips;
    if (!clientIp.empty())
        boost::split(ips, clientIp, boost::is_any_of(","));

    auto forwardedFor = request.get("X-Forwarded-For");

    std::vector<std::string_view> forwardedIps;
    if (!forwardedFor.empty())
        boost::split(forwardedIps, forwardedFor, boost::is_any_of(","));

    //Utils::insert(ips, forwardedIps);
    ips.insert(ips.end(), forwardedIps.begin(), forwardedIps.end());

    for (auto &ip : ips) {
        ip = boost::trim_copy(ip);

        if (!ip.empty() && !request.isPrivateIP(ip)) {
            return ip;
        }
    }

    return remoteAddr;
  } else {
    if (conf.isTrustedProxy(remoteAddr)) {
        auto forwardedFor = request.get(conf.originalIPHeader());
        forwardedFor = boost::trim_copy(forwardedFor);
        std::vector<std::string_view> forwardedIps;
        boost::split(forwardedIps, forwardedFor, boost::is_any_of(","));
        for (auto it = forwardedIps.rbegin(); it != forwardedIps.rend(); ++it) {
            *it = boost::trim_copy(*it);
            if (!it->empty()) {
                if (!conf.isTrustedProxy(*it)) {
                    return *it;
                } else {
                    /*
             * When the left-most address in a forwardedHeader is contained
             * within a trustedProxy subnet, it should be returned as the clientAddress
             */
                    remoteAddr = *it;
                }
            }
        }
    }
    return remoteAddr;
  }
}

void WebSession::setLoaded()
{
  bool wasSuspended = state_ == State::Suspended;
  setState(State::Loaded, controller_->configuration().sessionTimeout());

  if (wasSuspended) {
    if (env_->ajax() && controller_->configuration().reloadIsNewSession()) {
      app_->doJavaScript(WT_CLASS ".history.removeSessionId()");
      sessionIdInUrl_ = false;
    }
    app_->unsuspended().emit();
  }
}

void WebSession::setExpectLoad()
{
  if (controller_->configuration().ajaxPuzzle())
    setState(State::ExpectLoad, controller_->configuration().bootstrapTimeout());
  else
    setLoaded();
}

void WebSession::setState(State state, int timeout)
{
#ifdef WT_THREADED
  // this assertion is not true for when we are working from an attached
  // thread: that thread does not have an associated handler, but its contract
  // dictates that it should work on behalf of a thread that has the lock.
  //assert(WebSession::Handler::instance()->haveLock());
#endif // WT_THREADED

  if (state_ != State::Dead) {
    state_ = state;

    LOG_DEBUG("Setting to expire in {}s", timeout);

#ifndef WT_TARGET_JAVA
    if (controller_->configuration().sessionTimeout() != -1)
      expire_ = Time() + timeout*1000;
#endif // WT_TARGET_JAVA
  }
}

std::string WebSession::sessionQuery() const
{
  std::string result ="?wtd=" + DomElement::urlEncodeS(sessionId_);
  if (type() == EntryPointType::WidgetSet)
    result += "&wtt=widgetset";
  return result;
}

void WebSession::init(const WebRequest& request)
{
  env_->init(request);

  const std::string *hashE = request.getParameter("_");

  absoluteBaseUrl_ = env_->urlScheme() + "://" + env_->hostName() + basePath_;

  bool useAbsoluteUrls;
#ifndef WT_TARGET_JAVA
  useAbsoluteUrls = env_->server()->readConfigurationProperty("baseURL", absoluteBaseUrl_);
#else
  std::string* absoluteBaseUrl 
    = app_->readConfigurationProperty("baseURL", absoluteBaseUrl_);
  if (absoluteBaseUrl != &absoluteBaseUrl_) {
    absoluteBaseUrl_ = *absoluteBaseUrl;
    useAbsoluteUrls = true;
  } else {
    useAbsoluteUrls = false;
  }
#endif

  if (useAbsoluteUrls) {
    std::string::size_type slashpos = absoluteBaseUrl_.rfind('/');
    if (slashpos != std::string::npos
	&& slashpos != absoluteBaseUrl_.length() - 1)
      absoluteBaseUrl_ = absoluteBaseUrl_.substr(0, slashpos + 1);

    slashpos = absoluteBaseUrl_.find("://");

    if (slashpos != std::string::npos) {
      slashpos = absoluteBaseUrl_.find("/", slashpos + 3);
      if (slashpos != std::string::npos) {
	deploymentPath_ = absoluteBaseUrl_.substr(slashpos) + applicationName_;
      }
    }
  }

  bookmarkUrl_ = applicationName_;

  if (type() == EntryPointType::WidgetSet || useAbsoluteUrls) {
    applicationUrl_ = absoluteBaseUrl_ + applicationName_;
    bookmarkUrl_ = applicationUrl_;
  }

  std::string path = request.pathInfo();
  if (path.empty() && hashE)
    path = *hashE;

  env_->setInternalPath(path);
  pagePathInfo_ = request.pathInfo();

  // Cache document root
  docRoot_ = getCgiValue("DOCUMENT_ROOT");
}

bool WebSession::useUglyInternalPaths() const
{
#ifndef WT_TARGET_JAVA
  /*
   * We need ugly ?_= internal paths if the server does not route
   * /app/foo to an application deployed as /app/
   */
  if (applicationName_.empty() && controller_->server()) {
    Configuration& conf = controller_->configuration();
    return conf.useSlashExceptionForInternalPaths();
  } else
    return false;
#else
  return false;
#endif
}

std::string WebSession::bootstrapUrl(const WebResponse& response, BootstrapOption option) const
{
  switch (option) {
  case BootstrapOption::KeepInternalPath: {
    std::string url;

    std::string internalPath
      = app_ ? app_->internalPath() : env_->internalPath();

    if (useUglyInternalPaths()) {
      if (internalPath.length() > 1)
	url = "?_=" + DomElement::urlEncodeS(internalPath, "#/");

      if (isAbsoluteUrl(applicationUrl_))
	url = applicationUrl_ + url;
    } else {
      if (!isAbsoluteUrl(applicationUrl_)) {
	/*
	 * Java application servers use ";jsessionid=..." which generates
	 * URLs relative to the current directory, not current filename
	 * (unlike '?=...')
	 *
	 * Therefore we start with the current 'filename', this does no harm
	 * for C++ well behaving servers either.
	 */
	if (internalPath.length() > 1) {
      std::string lastPart = internalPath.substr(internalPath.rfind('/') + 1);

	  url = ""; /* lastPart; */
	} else
	  url = applicationName_;
      } else {
	if (applicationName_.empty() && internalPath.length() > 1)
	  internalPath = internalPath.substr(1);

	url = applicationUrl_ + internalPath;
      }
    }

    return appendSessionQuery(url);
  }
  case BootstrapOption::ClearInternalPath: {
    std::string url;
    if (applicationName_.empty()) {
      url = fixRelativeUrl(".");
      url = url.substr(0, url.length() - 1);
    } else
      url = fixRelativeUrl(applicationName_);

    return appendSessionQuery(url);
  }
  default:
    assert(false);
  }

  return std::string();
}

std::string WebSession::bootstrapUrl(http::context *context, BootstrapOption option) const
{
  switch (option) {
  case BootstrapOption::KeepInternalPath: {
    std::string url;

    std::string internalPath = app_ ? app_->internalPath() : env_->internalPath();

    if (useUglyInternalPaths()) {
      if (internalPath.length() > 1)
        url = "?_=" + DomElement::urlEncodeS(internalPath, "#/");

      if (isAbsoluteUrl(applicationUrl_))
        url = applicationUrl_ + url;
    } else {
      if (!isAbsoluteUrl(applicationUrl_)) {
        /*
         * Java application servers use ";jsessionid=..." which generates
         * URLs relative to the current directory, not current filename
         * (unlike '?=...')
         *
         * Therefore we start with the current 'filename', this does no harm
         * for C++ well behaving servers either.
         */
        if (internalPath.length() > 1)
        {
          std::string lastPart = internalPath.substr(internalPath.rfind('/') + 1);

          url = ""; /* lastPart; */
        } else {
            url = applicationName_;
        }
      } else {
        if (applicationName_.empty() && internalPath.length() > 1)
          internalPath = internalPath.substr(1);

        url = applicationUrl_ + internalPath;
      }
    }

    return appendSessionQuery(url);
  }
  case BootstrapOption::ClearInternalPath:
  {
    std::string url;
    if (applicationName_.empty()) {
      url = fixRelativeUrl(".");
      url = url.substr(0, url.length() - 1);
    } else
      url = fixRelativeUrl(applicationName_);

    return appendSessionQuery(url);
  }
  default:
    assert(false);
  }

  return std::string();
}

std::string WebSession::fixRelativeUrl(const std::string& url) const
{
  if (isAbsoluteUrl(url))
    return url;

  if (url.length() > 0 && url[0] == '#') {
    if (!isAbsoluteUrl(applicationUrl_))
      return url;
    else
      // we have <href base=...> which requires us to put the application
      // name before a named anchor
      return applicationName_ + url;
  }

  if (!isAbsoluteUrl(applicationUrl_))
  {
    if (!url.empty() && url[0] == '/')
      return url;
    else if (!env_->publicDeploymentPath_.empty()) {
      std::string dp = env_->publicDeploymentPath_;

      if (url.empty())
        return dp;
      else if (url[0] == '?')
        return dp + url;
      else {
        std::size_t s = dp.rfind('/');
        std::string parentDir = dp.substr(0, s + 1);
        if (url[0] == '.' && (url.size() == 1 || url[1] == '?' || url[1] == '#' || url[1] == ';'))
          return parentDir + url.substr(1);
        else if (url.size() >= 2 && url[0] == '.' && url[1] == '/') {
          // Note: deployment path is guaranteed to start with /
          //       WEnvironment checks this!
          return parentDir + url.substr(2);
        } else
          return parentDir + url;
      }
    } else {
      /*
       * The public deployment path may lack if:
       *  - we are a widget set script, but then we should have absolute
       *    applicationUrl and internal paths are not really going to work
       *  - we are a plain HTML session. but then we are not hashing internal
       *    paths, so first condition should never be met
       */
      if (env_->internalPathUsingFragments())
        return url;
      else {
        std::string rel = "";
        std::string pi = pagePathInfo_;

        for (unsigned i = 0; i < pi.length(); ++i) {
          if (pi[i] == '/')
            rel += "../";
        }

        if (url.empty())
          return rel + applicationName_;
        else
          return rel + url;
      }
    }
  } else
    return makeAbsoluteUrl(url);
}

std::string WebSession::makeAbsoluteUrl(const std::string& url) const
{
  if (isAbsoluteUrl(url))
    return url;
  else {
    if (!url.empty() && url[0] == '.' &&
        (url.length() == 1 || url[1] != '.'))
      return absoluteBaseUrl_ + (url.c_str() + 1);
    else if (url.empty() || url[0] != '/')
      return absoluteBaseUrl_ + url;
    else
      return host(absoluteBaseUrl_) + url;
  }
}

std::string WebSession::mostRelativeUrl(const std::string& internalPath) const
{
  return appendSessionQuery(bookmarkUrl(internalPath));
}

std::string WebSession::appendSessionQuery(const std::string& url) const
{
  std::string result = url;
  
  if (env_->agentIsSpiderBot())
    return result;

  std::size_t questionPos = result.find('?');

  if (questionPos == std::string::npos)
    result += sessionQuery();
  else if (questionPos == result.length() - 1)
    result += sessionQuery().substr(1);
  else
    result += '&' + sessionQuery().substr(1);

#ifndef WT_TARGET_JAVA
  return result;
#else
  if (boost::starts_with(result, "?"))
    result = applicationUrl_ + result;

  if (WebSession::Handler::instance()->response())
    return WebSession::Handler::instance()->response()->encodeURL(result);
  else {
    /*
     * This may happen if we are inside a WServer::posted() function.
     * Unfortunately, then we cannot use Servlet API to URL encode.
     */
    questionPos = result.find('?');
    return result.substr(0, questionPos) + ";jsessionid=" + sessionId()
      + result.substr(questionPos);
  }
#endif // WT_TARGET_JAVA
}

std::string WebSession::bookmarkUrl() const
{
  if (app_)
    return bookmarkUrl(app_->internalPath());
  else
    return bookmarkUrl(env_->internalPath());
}

std::string WebSession::bookmarkUrl(const std::string& internalPath) const
{
  std::string result = bookmarkUrl_;

  return appendInternalPath(result, internalPath);
}

std::string WebSession::appendInternalPath(const std::string& baseUrl, const std::string& internalPath) const
{
  if (internalPath.empty() || internalPath == "/")
    if (baseUrl.empty())
      if (applicationName_.empty())
	return ".";
      else
        return applicationName_;
    else
      return baseUrl;
  else {
    if (useUglyInternalPaths())
      return baseUrl + "?_=" + DomElement::urlEncodeS(internalPath, "#/");
    else {
      if (applicationName_.empty())
	return baseUrl + DomElement::urlEncodeS(internalPath.substr(1), "#/");
      else
	return baseUrl + DomElement::urlEncodeS(internalPath, "#/");
    }
  }
}

bool WebSession::start(WebResponse *response, EntryPoint *ep)
{
  try {
    app_ = controller_->doCreateApplication(this, ep).release();
    if (!app_->internalPathValid_)
      if (response->responseType() == WebResponse::ResponseType::Page)
            response->setStatus(404);
  } catch (std::exception& e) {
    app_ = nullptr;

    kill();
    RETHROW(e);
  } catch (...) {
    app_ = nullptr;

    kill();
    throw;
  }

  return app_;
}

void WebSession::init(Wt::http::context *context)
{
  env_->init(context);

  auto hashE = context->getParameter("_");

  absoluteBaseUrl_ = env_->urlScheme() + "://" + env_->hostName() + basePath_;

  bool useAbsoluteUrls;
#ifndef WT_TARGET_JAVA
  useAbsoluteUrls = env_->server()->readConfigurationProperty("baseURL", absoluteBaseUrl_);
#else
  std::string* absoluteBaseUrl
      = app_->readConfigurationProperty("baseURL", absoluteBaseUrl_);
  if (absoluteBaseUrl != &absoluteBaseUrl_) {
    absoluteBaseUrl_ = *absoluteBaseUrl;
    useAbsoluteUrls = true;
  } else {
    useAbsoluteUrls = false;
  }
#endif

  if (useAbsoluteUrls)
  {
    std::string::size_type slashpos = absoluteBaseUrl_.rfind('/');
    if (slashpos != std::string::npos
        && slashpos != absoluteBaseUrl_.length() - 1)
      absoluteBaseUrl_ = absoluteBaseUrl_.substr(0, slashpos + 1);

    slashpos = absoluteBaseUrl_.find("://");

    if (slashpos != std::string::npos)
    {
      slashpos = absoluteBaseUrl_.find("/", slashpos + 3);
      if (slashpos != std::string::npos) {
            deploymentPath_ = absoluteBaseUrl_.substr(slashpos) + applicationName_;
      }
    }
  }

  bookmarkUrl_ = applicationName_;

  if (type() == EntryPointType::WidgetSet || useAbsoluteUrls) {
    applicationUrl_ = absoluteBaseUrl_ + applicationName_;
    bookmarkUrl_ = applicationUrl_;
  }

  auto path = context->pathInfo(basePath_);// request.pathInfo();
  if (path.empty() && !hashE.empty())
    path = hashE;

  std::cout << "env_->setInternalPath " << path << std::endl;

  env_->setInternalPath(std::string(path));
  pagePathInfo_ = context->pathInfo(); //request.pathInfo();

  // Cache document root
  docRoot_ = getCgiValue("DOCUMENT_ROOT");
}

awaitable<bool> WebSession::start(http::context *context, EntryPoint *ep)
{
  try {
    //app_ = co_await ep->appCallback()(this->env()).release();
    app_ = (co_await ep->appCallback()(this->env())).release(); // controller_->doCreateApplication(this, ep).release();
    if (!app_->internalPathValid_)
      if (context->responseType() == http::ResponseType::Page)
        context->status(404);
  } catch (std::exception& e) {
    app_ = nullptr;

    kill();
    RETHROW(e);
  } catch (...) {
    app_ = nullptr;

    kill();
    throw;
  }

  co_return app_;
}

std::string WebSession::getCgiValue(const std::string& varName) const
{
  WebRequest *request = WebSession::Handler::instance()->request();
  if (request)
    return str(request->envValue(varName.c_str()));
  else if(varName == "DOCUMENT_ROOT")
	return docRoot_;
  else 
    return std::string();
}

std::string WebSession::getCgiHeader(const std::string& headerName) const
{
  WebRequest *request = WebSession::Handler::instance()->request();
  if (request)
    return str(request->headerValue(headerName.c_str()));
  else
    return std::string();
}

void WebSession::kill()
{
  state_ = State::Dead;

  /*
   * Unlock the recursive eventloop that may be pending.
   */
  unlockRecursiveEventLoop();
}

void WebSession::checkTimers()
{
  WContainerWidget *timers = app_->timerRoot();

  const std::vector<WWidget *>& timerWidgets = timers->children();

  std::vector<WTimerWidget *> expired;

  for (unsigned i = 0; i < timerWidgets.size(); ++i) {
    WTimerWidget *wti = dynamic_cast<WTimerWidget *>(timerWidgets[i]);

    if (wti->timerExpired())
      expired.push_back(wti);
  }

  WMouseEvent dummy;

  for (unsigned i = 0; i < expired.size(); ++i)
    expired[i]->clicked().emit(dummy);
}

void WebSession::redirect(const std::string& url)
{
  redirect_ = url;
  if (redirect_.empty())
    redirect_ = "?";
}

std::string WebSession::getRedirect()
{
  std::string result = redirect_;
  redirect_.clear();
  return result;
}

WebSession::Handler::Handler()
  : nextSignal(-1),
    prevHandler_(nullptr),
    session_(nullptr),
    request_(nullptr),
    response_(nullptr),
    killed_(false)
{
  init();
}

WebSession::Handler::Handler(const std::shared_ptr<WebSession>& session, LockOption lockOption)
  : nextSignal(-1),
#ifndef WT_TARGET_JAVA
    sessionPtr_(session),
#endif // WT_TARGET_JAVA
#ifdef WT_THREADED
    lock_(session->mutex_, std::defer_lock),
#endif // WT_THREADED
    prevHandler_(nullptr),
    session_(session.get()),
    request_(nullptr),
    response_(nullptr),
    killed_(false)
{
  switch (lockOption) {
  case LockOption::NoLock:
    break;
  case LockOption::TakeLock:
#ifdef WT_THREADED
    lockOwner_ = std::this_thread::get_id();
    lock_.lock();
#endif
#ifdef WT_TARGET_JAVA
    session->mutex().lock();
#endif
    break;
  case LockOption::TryLock:
#ifdef WT_THREADED
    if (lock_.try_lock())
      lockOwner_ = std::this_thread::get_id();
#endif
#ifdef WT_TARGET_JAVA
    session->mutex().try_lock();
#endif
    break;
  }

  init();
}

WebSession::Handler::Handler(WebSession *session)
  : nextSignal(-1),
#ifdef WT_THREADED
    lock_(session->mutex_),
#endif // WT_THREADED
    prevHandler_(nullptr),
    session_(session),
    request_(nullptr),
    response_(nullptr),
    killed_(false)
{
#ifdef WT_THREADED
  lockOwner_ = std::this_thread::get_id();
#endif
#ifdef WT_TARGET_JAVA
  session->mutex().lock();
#endif // WT_TARGET_JAVA

  init();
}

WebSession::Handler::Handler(const std::shared_ptr<WebSession>& session,
                             WebRequest& request, WebResponse& response)
  : nextSignal(-1),  
#ifndef WT_TARGET_JAVA
    sessionPtr_(session),
#endif // WT_TARGET_JAVA
#ifdef WT_THREADED
    lock_(session->mutex_),
#endif // WT_THREADED
    prevHandler_(nullptr),
    session_(session.get()),
    request_(&request),
    response_(&response),
    killed_(false)
{
#ifdef WT_THREADED
  lockOwner_ = std::this_thread::get_id();
#endif
#ifdef WT_TARGET_JAVA
  session->mutex().lock();
#endif

  init();
}

WebSession::Handler::Handler(const std::shared_ptr<WebSession> &session, Wt::http::context *context)
    : nextSignal(-1),
#ifndef WT_TARGET_JAVA
    sessionPtr_(session),
#endif // WT_TARGET_JAVA
#ifdef WT_THREADED
    lock_(session->mutex_),
#endif // WT_THREADED
    prevHandler_(nullptr),
    session_(session.get()),
    context_(context),
    request_(nullptr),
    response_(nullptr),
    killed_(false)
{
#ifdef WT_THREADED
  lockOwner_ = std::this_thread::get_id();
#endif
#ifdef WT_TARGET_JAVA
  session->mutex().lock();
#endif

  init();
}

WebSession::Handler *WebSession::Handler::instance()
{
#ifdef WT_TARGET_JAVA
  return threadHandler_.get();
#else
  return threadHandler_;
#endif
}

bool WebSession::Handler::haveLock() const
{
#ifdef WT_THREADED
  return lock_.owns_lock();
#else
#ifdef WT_TARGET_JAVA
  return session_->mutex().owns_lock();
#else
  return true;
#endif
#endif
}

void WebSession::Handler::unlock()
{
#ifndef WT_TARGET_JAVA
  if (haveLock()) {
    Utils::erase(session_->handlers_, this);
#ifdef WT_THREADED
    lock_.unlock();
#endif // WT_THREADED
  }
#endif // WT_TARGET_JAVA
}

void WebSession::Handler::init()
{
  prevHandler_ = attachThreadToHandler(this);

#ifndef WT_TARGET_JAVA
  if (haveLock())
    session_->handlers_.push_back(this);
#endif
}

WebSession::Handler *
WebSession::Handler::attachThreadToHandler(Handler *handler)
{
  WebSession::Handler *result;

#ifdef WT_TARGET_JAVA
  result = threadHandler_.release();
  threadHandler_.reset(handler);
#else
  result = threadHandler_;
  threadHandler_ = handler;
#endif

  return result;
}

bool WebSession::attachThreadToLockedHandler()
{
#if !defined(WT_TARGET_JAVA)
  /*
   * We assume that another handler has already locked this session for us.
   * We just need to find it.
   */
  for (unsigned i = 0; i < handlers_.size(); ++i)
    if (handlers_[i]->haveLock()) {
      WebSession::Handler::attachThreadToHandler(handlers_[i]);
      return true;
    }

  return false;
#else
  Handler::attachThreadToHandler(new Handler(this, Handler::LockOption::NoLock));
  return true;
#endif
}

void WebSession
::Handler::attachThreadToSession(const std::shared_ptr<WebSession>& session)
{
  attachThreadToHandler(nullptr);

#ifdef WT_BOOST_THREADS
  if (!session.get())
    return;

  /*
   * It may be that we still need to attach to a session while it is being
   * destroyed ? I'm not sure why this is useful, but cannot see anything
   * wrong about it either ?
   */
  if (session->state_ == State::Dead)
    LOG_WARN_S(session, "attaching to dead session?");

  if (!session.get()->attachThreadToLockedHandler()) {
    /*
     * We actually have two scenarios:
     * - attachThread() once to have WApplication::instance() work. This will
     *   give the warning, and will not work reliably !
     * - attachThread() in the wtwithqt case should execute what we have above
     */
    LOG_WARN_S(session,
           "attachThread(): no thread is holding this application's lock ?");
    WebSession::Handler::attachThreadToHandler(new Handler(session, Handler::LockOption::NoLock));
  }
#else
  LOG_ERROR_S(session, "attachThread(): needs Wt built with threading enabled");
#endif
}

std::shared_ptr<ApplicationEvent> WebSession::popQueuedEvent()
{
#ifdef WT_BOOST_THREADS
#ifndef WT_TARGET_JAVA
  std::unique_lock<std::mutex> lock(eventQueueMutex_);
#else
  eventQueueMutex_.lock();
#endif // WT_TARGET_JAVA
#endif // WT_BOOST_THREADS

  std::shared_ptr<ApplicationEvent> result;

  LOG_DEBUG("popQueuedEvent(): {}", eventQueue_.size());

  if (!eventQueue_.empty()) {
    result = eventQueue_.front();
    eventQueue_.pop_front();
  }

#ifdef WT_TARGET_JAVA
  eventQueueMutex_.unlock();
#endif // WT_TARGET_JAVA

  return result;
}

void WebSession::queueEvent(const std::shared_ptr<ApplicationEvent>& event)
{
#ifdef WT_BOOST_THREADS
#ifndef WT_TARGET_JAVA
  std::unique_lock<std::mutex> lock(eventQueueMutex_);
#else
  eventQueueMutex_.lock();
#endif // WT_TARGET_JAVA
#endif // WT_BOOST_THREADS

  eventQueue_.push_back(event);

  LOG_DEBUG("queueEvent(): {}", eventQueue_.size());

#ifdef WT_TARGET_JAVA
  eventQueueMutex_.unlock();
#endif // WT_TARGET_JAVA
}

awaitable<void> WebSession::processQueuedEvents(WebSession::Handler& handler)
{
  for (;;) {
    std::shared_ptr<ApplicationEvent> event = popQueuedEvent();

    if (event) {
      if (!dead()) {
        co_await externalNotify(WEvent::Impl(&handler, event->function));

	if (app() && app()->hasQuit())
	  kill();

	if (dead())
          controller()->removeSession(event->sessionId);
      } else {
        if (event->fallbackFunction)
          WT_CALL_FUNCTION(event->fallbackFunction);
      }
    } else
      break;
  }
  co_return;
}

#ifdef WT_TARGET_JAVA
void WebSession::Handler::release()
{
  if (haveLock()) {
    session_->processQueuedEvents(*this);
    if (session_->triggerUpdate_)
      session_->pushUpdates();
    session_->mutex().unlock();
  }

  attachThreadToHandler(prevHandler_);
}
#endif

WebSession::Handler::~Handler()
{
#ifndef WT_TARGET_JAVA
//  if (haveLock()) {
//    /* We should check that the session state is not dead ? */
//    session_->processQueuedEvents(*this);
//    if (session_->triggerUpdate_)
//      session_->pushUpdates();
//    else if (response_ && session_->state_ != State::Dead)
//      session()->render(*this);

//    Utils::erase(session_->handlers_, this);
//  }
  if(!destroyed)
    LOG_ERROR("No destroy call");

  if (session_->handlers_.empty())
    session_->hibernate();

  attachThreadToHandler(prevHandler_);
#endif // WT_TARGET_JAVA
}

awaitable<void> WebSession::Handler::destroy()
{
  destroyed = true;
  if (haveLock()) {
    /* We should check that the session state is not dead ? */
    co_await session_->processQueuedEvents(*this);
    if (session_->triggerUpdate_)
      session_->pushUpdates();
    else if (response_ && session_->state_ != State::Dead)
      session()->render(*this);

    Utils::erase(session_->handlers_, this);
  }
  co_return;
}

void WebSession::Handler::setRequest(WebRequest *request, WebResponse *response)
{
  request_ = request;
  response_ = response;
}

void WebSession::Handler::setRequest(http::context *context)
{
  context_ = context;
}

void WebSession::Handler::flushResponse()
{
  if(context_->isWebSocketMessage()) {
    session_->pushUpdates();
    //return;
  }
  if(context_) {
    context_->flush();
    setRequest(nullptr, nullptr);
    setRequest(nullptr);
  }
//  if (response_) {
//    response_->flush();
//    setRequest(nullptr, nullptr);
//  }
}

void WebSession::hibernate()
{
  if (app_ && app_->localizedStrings_)
    app_->localizedStrings_->hibernate();
}

EventSignalBase *WebSession::decodeSignal(const std::string& signalId, bool checkExposed) const
{
  EventSignalBase *result = app_->decodeExposedSignal(signalId);

  if (result && checkExposed) {
    WWidget *w = dynamic_cast<WWidget *>(result->owner());
    if (w && !app_->isExposed(w))
      result = nullptr;
  }

  if (!result && checkExposed) {
    if (app_->justRemovedSignals().find(signalId) == app_->justRemovedSignals().end())
      LOG_ERROR("decodeSignal(): signal '{}' not exposed", signalId);
  }

  return result;
}

EventSignalBase *WebSession::decodeSignal(const std::string& objectId,
                                          const std::string& name,
                                          bool checkExposed) const
{
  std::string signalId = app_->encodeSignal(objectId, name);

  return decodeSignal(signalId, checkExposed && name != "resized");
}

WebSession *WebSession::instance()
{
  Handler *handler = WebSession::Handler::instance();
  return handler ? handler->session() : nullptr;
}
#warning "hanlder need call destroy & propagate co_await"
awaitable<void> WebSession::doRecursiveEventLoop()
{
  Handler *handler = WebSession::Handler::instance();

#ifndef WT_BOOST_THREADS
  LOG_ERROR("cannot do recursive event loop without threads");
#else

#ifdef WT_TARGET_JAVA
  if (handler->request() && !WebController::isAsyncSupported())
    throw WException("Recursive eventloop requires a Servlet 3.0 "
		     "enabled servlet container and an application "
		     "with async-supported enabled.");
#endif

  /*
   * Finish the request that is being handled
   *
   * It could be that handler does not have a request/response:
   *  if it is actually a long polling server push request.
   *  if we are somehow recursing recursive event loops: e.g. 
   *    processEvents() during exec()
   *
   * In that case, we do not need to finish it.
   */
  if (handler->request())
    handler->session()->notifySignal(WEvent(WEvent::Impl(handler)));
  else
    if (app_->updatesEnabled())
      app_->triggerUpdate();

  if (handler->response())
    handler->session()->render(*handler);

  if (state_ == State::Dead) {
    recursiveEventHandler_ = nullptr;
    throw WException("doRecursiveEventLoop(): session was killed");
  }

  /*
   * Register that we are doing a recursive event loop, this is used in
   * handleRequest() to let the recursive event loop do the actual
   * notification.
   */
  Handler *prevRecursiveEventHandler = recursiveEventHandler_;
  recursiveEventHandler_ = handler;
  newRecursiveEvent_ = nullptr;

  /*
   * Release session mutex lock, wait for recursive event, and retake
   * the session mutex lock.
   */
#ifndef WT_TARGET_JAVA
//  if (webSocket_)
//    webSocket_->readWebSocketMessage(std::bind(&WebSession::handleWebSocketMessage, shared_from_this(), std::placeholders::_1));

//  if (controller_->server()->ioService().requestBlockedThread()) {
//    while (!newRecursiveEvent_)
//      try {
//        recursiveEvent_.wait(handler->lock());
//      } catch (...) {
//        controller_->server()->ioService().releaseBlockedThread();
//        throw;
//      }
//    controller_->server()->ioService().releaseBlockedThread();
//  } else {
//    // Allow at least one thread to serve requests in order to avoid a
//    // locked-up Wt. Even worse, Wt deadlocks if all threads are
//    // occupied in internal event loops and all those browser windows
//    // are closed (session time out does not work anymore)
//    throw WException("doRecursiveEventLoop(): all threads are busy. Avoid using recursive event loops.");
//  }
#else
  while (!newRecursiveEvent_)
    recursiveEvent_.wait();
#endif

  if (state_ == State::Dead) {
    recursiveEventHandler_ = nullptr;
    delete newRecursiveEvent_;
    newRecursiveEvent_ = nullptr;
    throw WException("doRecursiveEventLoop(): session was killed");
  }

  setLoaded();

  /*
   * We use recursiveEventHandler_ != 0 to postpone rendering: we only want
   * the event handling part.
   */
  co_await app_->notify(WEvent(*newRecursiveEvent_));
  delete newRecursiveEvent_;
  newRecursiveEvent_ = nullptr;
  recursiveEventDone_.notify_one();

  recursiveEventHandler_ = prevRecursiveEventHandler;
#endif // WT_BOOST_THREADS
}

void WebSession::expire()
{
  kill();
}

bool WebSession::unlockRecursiveEventLoop()
{
  if (!recursiveEventHandler_)
    return false;

  /*
   * Pass on the handler to the recursive event loop.
   */
  Handler *handler = WebSession::Handler::instance();

  recursiveEventHandler_->setRequest(handler->request(), handler->response());
  handler->setRequest(nullptr, nullptr);

  recursiveEventHandler_->setRequest(handler->context());
  handler->setRequest(nullptr);

  newRecursiveEvent_ = new WEvent::Impl(recursiveEventHandler_);

#ifdef WT_BOOST_THREADS
  recursiveEvent_.notify_one();
#endif

  return true;
}

awaitable<void> WebSession::handleRequest(Handler& handler, EntryPoint *ep)
{
  auto context = handler.context_;

  auto& request = context->req();

  auto wtdE = context->getParameter("wtd");
  auto origin = context->origin();

  Configuration& conf = controller_->configuration();

  //const char *origin = request.headerValue("Origin");
  if (request.websocket())
  {
    std::string trustedOrigin = env_->urlScheme() + "://" + env_->hostName();
    // Allow new WebSocket connection:
    // - Origin is OK if:
    //  - It is the same as the current host
    //  - or we are using WidgetSet mode and the origin is allowed
    // - Wt session id matches
    if ((trustedOrigin == origin ||
         (type() == EntryPointType::WidgetSet && conf.isAllowedOrigin(origin))) && wtdE == sessionId_) {
      // OK
    } else {
      // Not OK
      if (!origin.empty()) {
        LOG_ERROR("WebSocket request refused: Origin '{}' not allowed (trusted origin is '{}')", origin, trustedOrigin);
      } else {
        LOG_ERROR("WebSocket request refused: missing Origin");
      }
//      context->status(403);
//      context->flush();
      handler.context()->status(403);
      handler.flushResponse();
      co_return;
    }
  }
  else if (!origin.empty())
  {
    /*
     * CORS (Cross-Origin Resource Sharing)
     */
    /*
     * Do we allow this XMLHttpRequest or WebSocketRequest?
     *
     * Only if all of the conditions below are met:
     *  - this is a WidgetSet sessions
     *  - the Origin is allowed according to the configuration
     *  - this is a new session or the session id matches
     */
    if (type() == EntryPointType::WidgetSet && ((wtdE == sessionId_) || state_ == State::JustCreated) && conf.isAllowedOrigin(origin))
    {
      if (origin == "null")
        origin = "*";

      context->addHeader("Access-Control-Allow-Origin", origin);
      context->addHeader("Access-Control-Allow-Credentials", "true");
      context->addHeader("Vary", "Origin");

      if (context->method() == "OPTIONS") {
        context->status(200);
        context->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
        context->addHeader("Access-Control-Max-Age", "1728000");

        auto requestHeaders = context->getParameter("Access-Control-Request-Headers");

        if (!requestHeaders.empty())
          context->addHeader("Access-Control-Allow-Headers", requestHeaders);

        context->flush();
        co_return;
      }
    }
  }

  auto requestE = request.getParameter("request");
  bool requestForResource = resourceRequest(context);
  bool requestForStyle = requestE == "style";

  if (requestE == "ws" && !request.websocket())
  {
    LOG_ERROR("invalid WebSocket request, ignoring");

    LOG_INFO("Connection: {}", request.get("Connection"));
    LOG_INFO("Upgrade: {}", request.get("Upgrade"));
    LOG_INFO("Sec-WebSocket-Version: {}", request.get("Sec-WebSocket-Version"));

    //context->flush();
    handler.flushResponse();
    co_return;
  }

  if (request.websocket())
  {
#ifdef WT_TARGET_JAVA
    if (conf.webSockets()) {
      handler.response()->setStatus(500); // Internal Server Error
      handler.flushResponse();
      throw new WException("Server does not implement JSR-356 for WebSockets");
    }
#else
    if (state_ != State::JustCreated) {
      handleWebSocketRequest(handler);
      co_return;
    } else {
      //context->flush();
      handler.flushResponse();
      kill();
      co_return;
    }
#endif // WT_TARGET_JAVA
  }


  context->setResponseType(http::ResponseType::Page);

  /*
   * Only handle GET, POST and OPTIONS requests, unless a resource is
   * listening.
   */
  if (!(requestForResource
        || request.method() == "POST"
        || request.method() == "GET" || webSocketConnected_))
  {
    handler.context()->status(400);
    handler.flushResponse();
//    handler.response()->setStatus(400); // Bad Request
//    handler.flushResponse();
    co_return;
  }

  /*
   * If ajax session is already established, reject GET with wtd parameter
   * matching sessionId_, unless resource request or reloadIsNewSession() is false
   */
  if (env_->ajax()
      && request.method() == "GET"
      && !requestForResource
      && !requestForStyle
      && conf.reloadIsNewSession()
      && !suspended()
      && wtdE == sessionId_)
  {
    LOG_SECURE("Unexpected GET request with wtd of existing Ajax session");
    serveError(403, handler, "Forbidden");
    co_return;
  }

  /*
   * Under what circumstances do we allow a request which does not have
   * a session ID (i.e. who as it only through a cookie?)
   *  - when a new session is created
   *  - when reloading the page
   *
   * in other cases: discard the request
   */
  if ((wtdE.empty() || wtdE != sessionId_) && state_ != State::JustCreated && (requestE == "jsupdate" || requestE == "jserror" || requestE == "resource"))
  {
    //LOG_DEBUG("CSRF: {} != {}, requestE: {}", (wtdE ? *wtdE : "no wtd"), sessionId_, (requestE ? requestE : "none"));
    LOG_SECURE("CSRF prevention kicked in.");
    serveError(403, handler, "Forbidden");
  }
  else
    try {
      /*
       * If we have just created a new session, we need to take care:
       * - requests from a dead session -> reload
       * - otherwise: serve Boot.html, Hybrid.html or Plain.html
       *
       * Otherwise, we are Loaded: we need to react to:
       * - when missing a request: rerender (Plain or Hybrid)
       * - if request for 'script':
       *   (if appropriate, upgrade to Ajax)
       *     serve script
       * - if signal ...
       * - if resource ...
       */
      switch (state_) {
      case State::JustCreated:
      {
        if (conf.sessionTracking() == Configuration::Combined) {
          renderer().updateMultiSessionCookie(context);
        }

        switch (type_) {
        case EntryPointType::Application:
        {
          init(context); // env, url/internalpath

          // Handle requests from dead sessions:
          //
          // We need to send JS to reload the page when we get
          // 'request' == 'updatejs' or 'request' == "script"
          // We ignore 'request' == 'resource'
          //
          // In other cases we can simply start

          if (!requestE.empty()) {
            if (requestE == "jsupdate" ||
                requestE == "jserror" ||
                requestE == "script")
            {
                context->setResponseType(http::ResponseType::Update);
                LOG_INFO("signal from dead session, sending reload.");
                renderer_.letReloadJS(context, true);

                kill();
                break;
            } else if (requestE != "page") {
                LOG_INFO("Not serving this: request of type '{}' in a brand new session (probably coming from an old session)", requestE);
                context->setContentType("text/html");
                context->out() << "<html><head></head><body></body></html>";

                kill();
                break;
            }
          }

          /*
           * We can simply bootstrap.
           */
          {
            const std::string *internalPath = env_->getCookie("WtInternalPath");
            if (internalPath)
                env_->setInternalPath(*internalPath);
          }

          bool forcePlain = env_->agentIsSpiderBot() || !env_->agentSupportsAjax();

          progressiveBoot_ = !forcePlain && conf.progressiveBoot(env_->internalPath());

          if (forcePlain || progressiveBoot_)
          {
            /*
             * First start the application
             */
            if (!co_await start(context, ep))
                throw WException("Could not start application.");

            co_await app_->notify(WEvent(WEvent::Impl(&handler)));

            if (env_->agentIsSpiderBot())
                kill();
            else if (controller_->limitPlainHtmlSessions()) {
                LOG_SECURE("DoS: plain HTML sessions being limited");

                if (forcePlain)
                    kill();
                else // progressiveBoot_
                    setState(State::Loaded, conf.bootstrapTimeout());
            } else
                setLoaded();
          } else {
            /*
             * Delay application start
             */
            serveResponse(handler);
            setState(State::Loaded, conf.bootstrapTimeout());
          }
          break;
        }
        case EntryPointType::WidgetSet:
          if (requestForResource || requestForStyle) {
            auto resourceE = request.get("resource");
            if (resourceE == "blank") {
                context->type("text/html");
                context->out() <<
                    "<html><head><title>bhm</title></head>"
                    "<body> </body></html>";
            } else {
                LOG_INFO("not starting session for unexpected request type.");
                context->type("text/html");
                context->out() << "<html><head></head><body></body></html>";
            }

            kill();
          } else {
            context->setResponseType(http::ResponseType::Script);

            init(context); // env, url/internalpath, initial query parameters
            env_->enableAjax(context);

            if (!co_await start(context, ep))
                throw WException("Could not start application.");

            co_await app_->notify(WEvent(WEvent::Impl(&handler)));

            setExpectLoad();
          }
          break;
        default:
          assert(false); // EntryPointType::StaticResource
        }

        break;
      }
      case State::ExpectLoad:
      case State::Loaded:
      case State::Suspended:
      {
        if (conf.sessionTracking() == Configuration::Combined)
        {
          auto signalE = context->getParameter("signal");
          bool isKeepAlive = !requestE.empty() && signalE == "keepAlive";
          if (isKeepAlive || !env_->ajax()) {
            renderer().updateMultiSessionCookie(context);
          }
        }

        if (!requestE.empty())
        {
          if (requestE == "jsupdate" || requestE == "jserror")
            context->setResponseType(http::ResponseType::Update);
          else if (requestE == "script") {
            context->setResponseType(http::ResponseType::Script);
            if (state_ == State::Loaded)
                setExpectLoad();
          } else if (requestE == "style") {
            flushBootStyleResponse();

            auto page = context->getParameter("page");

            // See:
            // http://www.blaze.io/mobile/ios5-top10-performance-changes/
            // Mozilla/5.0 (iPad; CPU OS 5_1_1 like Mac OS X)
            //  AppleWebKit/534.46 (KHTML, like Gecko)
            //  Version/5.1 Mobile/9B206 Safari/7534.48.3
            bool ios5 = env_->agentIsMobileWebKit()
                        && (env_->userAgent().find("OS 5_") != std::string::npos
                            || env_->userAgent().find("OS 6_") != std::string::npos
                            || env_->userAgent().find("OS 7_") != std::string::npos
                            || env_->userAgent().find("OS 8_") != std::string::npos);

            // check js parameter
            auto jsE = context->getParameter("js");
            bool nojs = jsE == "no";

            bool bootStyle = (app_ || (!ios5 && !nojs)) &&
                             page == std::to_string(renderer_.pageId());

            if (!bootStyle)
            {
                handler.context()->type("text/css");
                handler.flushResponse();
            } else {
#ifndef WT_TARGET_JAVA
                if (!app_)
                {
                    bootStyleResponse_ = handler.context();
                    handler.setRequest(nullptr, nullptr);
                    handler.setRequest(nullptr);

                    controller_->server()
                        ->schedule(std::chrono::milliseconds{2000}, sessionId_,
                                   std::bind(&WebSession::flushBootStyleResponse, this));
                } else {
                    renderer_.serveLinkedCss(context);
                    handler.flushResponse();
                }
#else
            /*
           * In Servlet2, we cannot defer responding the request. So we
           * do a little spin lock here.
           *
           * There is no reason why the second request (script) does not
           * arrive within seconds.
           */
            unsigned i = 0;
            const unsigned MAX_TRIES = 1000;

            while (!app_ && i < MAX_TRIES) {
                mutex_.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                mutex_.lock();

                ++i;
            }

            if (i < MAX_TRIES) {
                renderer_.serveLinkedCss(context);
            }

            handler.flushResponse();
#endif // WT_TARGET_JAVA
            }

            break;
          }
        }

        if (!app_) {
          //const std::string *resourceE = request.getParameter("resource");
          auto resourceE = context->getParameter("resource");

          if (handler.context()->responseType() == http::ResponseType::Script)
          {
            if (request.get("skeleton").empty())
            {
                env_->enableAjax(context);

                if (!co_await start(handler.context(), ep))
                    throw WException("Could not start application.");
            } else {
                serveResponse(handler);
                co_return;
            }
          }
          else if (requestForResource && resourceE == "blank")
          {
            handler.context()->type("text/html");
            handler.context()->out() << "<html><head><title>bhm</title></head><body> </body></html>";
            break;
          }
          else
          {
            auto jsE = context->getParameter("js");

            if (jsE == "no") {
                if (!co_await start(handler.context(), ep))
                    throw WException("Could not start application.");

                if (controller_->limitPlainHtmlSessions()) {
                    LOG_SECURE("DoS: plain HTML sessions being limited");
                    kill();
                }
            }
            else
            {
                // This could be because the session Id was not as
                // expected. At least, it should be correct now.
                if (!conf.reloadIsNewSession() && wtdE == sessionId_) {
                    serveResponse(handler);
                    setState(State::Loaded, conf.bootstrapTimeout());
                } else {
                    handler.context()->type("text/html");
                    handler.context()->out() <<
                        "<html><body><h1>Refusing to respond.</h1></body></html>";
                }

                break;
            }
          }
        }

        bool doNotify = false;

        if (handler.context())
        {
          auto signalE = handler.context()->getParameter("signal");
          bool isPoll = signalE == "poll";

          if (requestForResource || isPoll || !unlockRecursiveEventLoop())
          {
            doNotify = true;

            if (env_->ajax()) {
                if (state_ != State::ExpectLoad &&
                    state_ != State::Suspended &&
                    handler.context()->responseType() == http::ResponseType::Update) {
                    setLoaded();
                }
            } else if (state_ != State::ExpectLoad &&
                       !(state_ == State::Suspended && requestForResource) &&
                       !controller_->limitPlainHtmlSessions()) {
                setLoaded();
            }
          }
        } else {
#ifndef WT_TARGET_JAVA
          doNotify = !app_->initialized_;
#else
          doNotify = false;
#endif
        }

        if (doNotify) {
          co_await app_->notify(WEvent(WEvent::Impl(&handler)));
          if (handler.context() && !requestForResource) {
            /*
             * This may be when an error was thrown during event
             * propagation: then we want to render the error message.
             */
            co_await app_->notify(WEvent(WEvent::Impl(&handler, true)));
          }
        }

        break;
      }
      case State::Dead:
        LOG_INFO("request to dead session, ignoring");
        break;
      }
    } catch (WException& e) {
      LOG_ERROR("fatal error: {}", e.what());

#ifdef WT_TARGET_JAVA
      e.printStackTrace();
#endif // WT_TARGET_JAVA

      kill();

      if (handler.context())
        serveError(500, handler, e.what());

    } catch (std::exception& e) {
      LOG_ERROR("fatal error: {}", e.what());

#ifdef WT_TARGET_JAVA
      e.printStackTrace();
#endif // WT_TARGET_JAVA

      kill();

      if (handler.context())
        serveError(500, handler, e.what());
    } catch (...) {
      LOG_ERROR("fatal error: caught unknown exception.");

      kill();

      if (handler.context())
        serveError(500, handler, "Unknown exception");
    }

  if (handler.context())
    handler.flushResponse();
  co_return;
}

void WebSession::flushBootStyleResponse()
{
  if (bootStyleResponse_) {
    bootStyleResponse_->flush();
    bootStyleResponse_ = nullptr;
  }
}

#ifndef WT_TARGET_JAVA
void WebSession::handleWebSocketRequest(Handler& handler)
{
  if (state_ != State::Loaded &&
      state_ != State::ExpectLoad &&
      state_ != State::Suspended)
  {
    handler.flushResponse();
    return;
  }

  /*
   * This triggers an orderly switch from Ajax to WebSocket:
   *
   *  First we ask for a 'connect', and in the mean time we do not yet
   *  use the socket. On the JS side, the connect disables any pending
   *  ajax long poll, and waits for the current pending response, if any.
   *  only then, we confirm the connect, transferring the last ackId.
   */
//  if (webSocket_) {
//    webSocket_->flush();
//    webSocket_ = nullptr;
//  }

  webSocket_ctx_ = handler.context();
  websocket_ = handler.context()->websocket_ptr();

  //webSocket_ = handler.context();
  canWriteWebSocket_ = false;
  webSocketConnected_ = false;

  //websocket_->send("");

//  webSocket_->flush
//      (WebRequest::ResponseState::ResponseFlush,
//       std::bind(&WebSession::webSocketConnect,
//                 std::weak_ptr<WebSession>(shared_from_this()),
//                 std::placeholders::_1));
  webSocket_ctx_->flush();

  //webSocket_ctx_->out() <<
  //websocket_->send("connect");

  handler.setRequest(nullptr, nullptr);
  handler.setRequest(nullptr);
}
#endif // WT_TARGET_JAVA

#ifndef WT_TARGET_JAVA
void WebSession::webSocketConnect(std::weak_ptr<WebSession> session, WebWriteEvent event)
{
  LOG_DEBUG("webSocketConnect()");

  std::shared_ptr<WebSession> lock = session.lock();
  if (lock) {
    Handler handler(lock, Handler::LockOption::TakeLock);

    if (!lock->webSocket_)
      return;

    switch (event) {
    case WebWriteEvent::Completed:
      lock->webSocket_->out() << "connect";

      lock->webSocket_->flush
          (WebRequest::ResponseState::ResponseFlush,
           std::bind(&WebSession::webSocketReady,
                     std::weak_ptr<WebSession>(lock),
                     std::placeholders::_1));

      lock->webSocket_->readWebSocketMessage
          (std::bind(&WebSession::handleWebSocketMessage,
                     std::weak_ptr<WebSession>(lock),
                     std::placeholders::_1));

      break;
    case WebWriteEvent::Error:
      lock->webSocket_->flush();
      lock->webSocket_ = nullptr;

      break;
    }
  }
  
}
#endif // WT_TARGET_JAVA

#ifdef WT_TARGET_JAVA
void WebSession::handleWebSocketMessage(Handler& handler)
{
  WebRequest *message = handler.request();
  bool closing = message->contentLength() == 0;

  if (!closing) {
    const std::string *connectedE = message->getParameter("connected");
    if (connectedE) {
      renderer_.ackUpdate(Utils::stoi(*connectedE));
      webSocketConnected_ = true;
      canWriteWebSocket_ = true;
    }

    const std::string *wsRqIdE = message->getParameter("wsRqId");
    if (wsRqIdE) {
      int wsRqId = Utils::stoi(*wsRqIdE);
      renderer_.addWsRequestId(wsRqId);
    }

    const std::string *signalE = message->getParameter("signal");

    if (signalE && *signalE == "ping") {
      LOG_DEBUG("ws: handle ping");
      if (canWriteWebSocket_) {
        webSocket_->out() << "{}";
        webSocket_->flushBuffer();
        return;
      }
    }

    const std::string *pageIdE = message->getParameter("pageId");

    if (pageIdE && *pageIdE != std::to_string(renderer_.pageId())) {
      closing = true;
    }

    if (!closing) {
      handleRequest(handler);
    } else
      webSocket_->flush();
    }
}
#endif // WT_TARGET_JAVA

#ifndef WT_TARGET_JAVA
void WebSession::handleWebSocketMessage(std::weak_ptr<WebSession> session, WebReadEvent event)
{
  LOG_DEBUG("handleWebSocketMessage: {}", (int)event);
  std::shared_ptr<WebSession> lock = session.lock();
  if (lock) {
    Handler handler(lock, Handler::LockOption::TakeLock);

    if (!lock->webSocket_)
      return;

    switch (event) {
    case WebReadEvent::Error:
      {
        if (lock->canWriteWebSocket_) {
          lock->webSocket_->flush();
          lock->webSocket_ = nullptr;
        }

        return;
      }

    case WebReadEvent::Ping:
      {
        WebSocketMessage *message = new WebSocketMessage(lock.get());

        if (lock->canWriteWebSocket_) {
          lock->canWriteWebSocket_ = false;
          lock->webSocket_->out() << "{}";
          lock->webSocket_->flush
            (WebRequest::ResponseState::ResponseFlush,
             std::bind(&WebSession::webSocketReady, session, std::placeholders::_1));
        }

        delete message;

        lock->webSocket_->readWebSocketMessage
          (std::bind(&WebSession::handleWebSocketMessage, session, std::placeholders::_1));

        break;
      }

    case WebReadEvent::Message:
      {
        WebSocketMessage *message = new WebSocketMessage(lock.get());

        bool closing = message->contentLength() == 0;

        if (!closing)
        {
          CgiParser cgi(lock->controller_->configuration().maxRequestSize(),
                        lock->controller_->configuration().maxFormDataSize());
          try {
            cgi.parse(*message, CgiParser::ReadDefault);
          } catch (std::exception& e) {
            LOG_ERROR("could not parse ws message: {}", e.what());
            closing = true;
          }
        }

        if (!closing)
        {
          const std::string *connectedE = message->getParameter("connected");
          if (connectedE) {
            if (lock->asyncResponse_) {
              lock->asyncResponse_->flush();
              lock->asyncResponse_ = nullptr;
            }

            lock->renderer_.ackUpdate(static_cast<unsigned int>(Utils::stoul(*connectedE)));
            lock->webSocketConnected_ = true;
          }

          const std::string *wsRqIdE = message->getParameter("wsRqId");
          if (wsRqIdE) {
            int wsRqId = Utils::stoi(*wsRqIdE);
            lock->renderer_.addWsRequestId(wsRqId);
          }

          const std::string *signalE = message->getParameter("signal");

          if (signalE && *signalE == "ping")
          {
            LOG_DEBUG("ws: handle ping");
            if (lock->canWriteWebSocket_)
            {
              lock->canWriteWebSocket_ = false;
              lock->webSocket_->out() << "{}";
              lock->webSocket_->flush
                  (WebRequest::ResponseState::ResponseFlush,
                   std::bind(&WebSession::webSocketReady, session, std::placeholders::_1));
            }

            lock->webSocket_->readWebSocketMessage
              (std::bind(&WebSession::handleWebSocketMessage, session, std::placeholders::_1));

            delete message;

            return;
          }

          const std::string *pageIdE = message->getParameter("pageId");
          if (pageIdE && *pageIdE != std::to_string(lock->renderer_.pageId()))
            closing = true;
        }

        if (!closing) {
          handler.setRequest(message, message);
          //lock->handleRequest(handler, nullptr);
        } else
          delete message;

        if (lock->dead()) {
          closing = true;
          lock->controller_->removeSession(lock->sessionId());
        }

        if (closing)
        {
          if (lock->webSocket_ && lock->canWriteWebSocket_)
          {
            lock->webSocket_->flush();
            lock->webSocket_ = nullptr;
          }
        } else
          if (lock->webSocket_)
            lock->webSocket_->readWebSocketMessage
              (std::bind(&WebSession::handleWebSocketMessage, session, std::placeholders::_1));
      }
    }
  }
}
#endif // WT_TARGET_JAVA


void WebSession::pushUpdates()
{
  LOG_DEBUG("pushUpdates()");

  triggerUpdate_ = false;

  if (!app_ || !renderer_.isDirty()) {
    LOG_DEBUG("pushUpdates(): nothing to do");
    return;
  }

  updatesPending_ = true;

  if (asyncResponse_)
  {
    asyncResponse_->setResponseType(http::ResponseType::Update);
    //co_await app_->notify(WEvent(WEvent::Impl(asyncResponse_)));
    {
        try
        {
            renderer_.serveResponse(asyncResponse_);
        }
        catch (std::exception& e)
        {
            LOG_ERROR("Exception in WApplication::notify(): {}", e.what());

#ifdef WT_TARGET_JAVA
            e.printStackTrace();
#endif // WT_TARGET_JAVA
        }
        catch (...)
        {
            LOG_ERROR("Exception in WApplication::notify()");
        }
        //co_return;
    }
    updatesPending_ = false;
    asyncResponse_->flush();
    asyncResponse_ = nullptr;
  }
  else if (webSocket_ctx_ && webSocketConnected_)
  {
//    if (webSocket_->webSocketMessagePending()) {
//      LOG_DEBUG("pushUpdates(): web socket message pending");
//      return;
//    }

//    if (canWriteWebSocket_)
//    {
#ifndef WT_TARGET_JAVA
      {
//        WebSocketMessage m(this);
//        m.setResponseType(WebResponse::ResponseType::Update);
        //app_->notify(WEvent(WEvent::Impl((WebResponse *)&m)));
        //co_await app_->notify(WEvent(WEvent::Impl(webSocket_ctx_)));
        {
            try
            {
            renderer_.serveResponse(webSocket_ctx_);
            }
            catch (std::exception& e)
            {
            LOG_ERROR("Exception in WApplication::notify(): {}", e.what());

#ifdef WT_TARGET_JAVA
            e.printStackTrace();
#endif // WT_TARGET_JAVA
            }
            catch (...)
            {
            LOG_ERROR("Exception in WApplication::notify()");
            }
            //co_return;
        }
      }

      updatesPending_ = false;
      canWriteWebSocket_ = false;

      webSocket_ctx_->flush();
//      webSocket_->flush
//          (WebRequest::ResponseState::ResponseFlush,
//           std::bind(&WebSession::webSocketReady,
//                     std::weak_ptr<WebSession>(shared_from_this()),
//                     std::placeholders::_1));
#else
      webSocket_->setResponseType(WebRequest::ResponseType::Update);
      co_await app_->notify(WEvent(WEvent::Impl(webSocket_)));
      updatesPending_ = false;
      webSocket_->flushBuffer();
#endif
//    }
  }

  if (updatesPending_) {
    LOG_DEBUG("pushUpdates(): cannot write now");
#ifdef WT_BOOST_THREADS
    updatesPendingEvent_.notify_one();
#endif
  }
}

#ifndef WT_TARGET_JAVA
void WebSession::webSocketReady(std::weak_ptr<WebSession> session, WebWriteEvent event)
{
  LOG_DEBUG("webSocketReady()");

  std::shared_ptr<WebSession> lock = session.lock();
  if (lock)
  {
    Handler handler(lock, Handler::LockOption::TakeLock);

    LOG_DEBUG("webSocketReady: webSocket_ = {} updatesPending = {} event = {}", (long long)lock->webSocket_, lock->updatesPending_, (int)event);

    switch (event) {
    case WebWriteEvent::Completed:
      if (lock->webSocket_) {
        lock->canWriteWebSocket_ = true;

        if (lock->updatesPending_)
          lock->pushUpdates();
      }

      break;
    case WebWriteEvent::Error:
      if (lock->webSocket_) {
        lock->webSocket_->flush();
        lock->webSocket_ = nullptr;
        lock->canWriteWebSocket_ = false;
      }

      break;
    }
  }
}
#endif // WT_TARGET_JAVA


std::string WebSession::ajaxCanonicalUrl(const WebResponse& request) const
{
  const std::string *hashE = nullptr;
  if (applicationName_.empty())
    hashE = request.getParameter("_");

  if (!pagePathInfo_.empty() || (hashE && hashE->length() > 1)) {
    std::string url;
    if (applicationName_.empty()) {
      url = fixRelativeUrl("?");
      url = url.substr(0, url.length() - 1);
    } else
      url = fixRelativeUrl(applicationName_);

    bool firstParameter = true;
    for (Http::ParameterMap::const_iterator i
         = request.getParameterMap().begin();
         i != request.getParameterMap().end(); ++i) {
      if (i->first != "_") {
        url += (firstParameter ? '?' : '&')
               + Utils::urlEncode(i->first) + '='
               + Utils::urlEncode(i->second[0]);
        firstParameter = false;
      }
    }

    url += '#' + (app_ ? app_->internalPath() : env_->internalPath());

    return url;
  } else
    return std::string();
}

std::string WebSession::ajaxCanonicalUrl(http::context *context) const
{
  std::string_view hashE;
  if (applicationName_.empty())
    hashE = context->getParameter("_");

  if (!pagePathInfo_.empty() || (hashE.length() > 1)) {
    std::string url;
    if (applicationName_.empty()) {
      url = fixRelativeUrl("?");
      url = url.substr(0, url.length() - 1);
    } else
      url = fixRelativeUrl(applicationName_);

    bool firstParameter = true;
    //auto headers = context->headers();
    auto& params = context->req().query();
    for(auto &[key, value] : params) {
      if (key != "_") {
        url += (firstParameter ? '?' : '&')
               + Utils::urlEncode(key) + '='
               + Utils::urlEncode(value);
        firstParameter = false;
      }
    }

    url += '#' + (app_ ? app_->internalPath() : env_->internalPath());

    return url;
  } else
    return std::string();
}

const std::string *WebSession::getSignal(const WebRequest& request,
                                         const std::string& se) const
{
  const std::string *signalE = request.getParameter(se + "signal");

  if (!signalE) {
    const int signalLength = 7 + se.length();

    const Http::ParameterMap& entries = request.getParameterMap();

    for (Http::ParameterMap::const_iterator i = entries.begin(); i != entries.end(); ++i)
    {
      if (i->first.length() > static_cast<unsigned>(signalLength)
          && i->first.substr(0, signalLength) == se + "signal=")
      {
        signalE = &i->second[0];

        std::string v = i->first.substr(signalLength);
        if (v.length() >= 2)
        {
          std::string e = v.substr(v.length() - 2);
          if (e == ".x" || e == ".y")
            v = v.substr(0, v.length() - 2);
        }

        *(const_cast<std::string *>(signalE)) = v;
        break;
      }
    }
  }

  return signalE;
}

std::string_view WebSession::getSignal(Wt::http::context *context, const std::string &se) const
{
  auto signalE = context->getParameter(se + "signal");

  if (signalE.empty()) {
    const int signalLength = 7 + se.length();

    auto& entries = context->req().query();

    for (auto& [key, value] : entries)
    {
      if (key.length() > static_cast<unsigned>(signalLength)
          && key.substr(0, signalLength) == se + "signal=")
      {
        signalE = value;

        auto v = key.substr(signalLength);
        if (v.length() >= 2)
        {
          auto e = v.substr(v.length() - 2);
          if (e == ".x" || e == ".y")
            v = v.substr(0, v.length() - 2);
        }

        signalE = v;
        break;
      }
    }
  }

  return signalE;
}

awaitable<void> WebSession::externalNotify(const WEvent::Impl& event)
{
  if (recursiveEventHandler_ && !newRecursiveEvent_)
  {
#ifdef WT_BOOST_THREADS
    newRecursiveEvent_ = new WEvent::Impl(event);
    recursiveEvent_.notify_one();
    while (newRecursiveEvent_) {
#ifdef WT_TARGET_JAVA
      recursiveEventDone_.wait();
#else
      recursiveEventDone_.wait(event.handler->lock());
#endif
    }
#endif
  }
  else
  {
    if (app_)
      co_await app_->notify(WEvent(event));
    else
      co_await notify(WEvent(event));
  }
}

awaitable<void> WebSession::notify(const WEvent& event)
{
  if (event.impl_.response)
  {
    try
    {
      renderer_.serveResponse(event.impl_.response);
    }
    catch (std::exception& e)
    {
      LOG_ERROR("Exception in WApplication::notify(): {}", e.what());

#ifdef WT_TARGET_JAVA
      e.printStackTrace();
#endif // WT_TARGET_JAVA
    }
    catch (...)
    {
      LOG_ERROR("Exception in WApplication::notify()");
    }
    co_return;
  }

  if (event.impl_.function) {
    try
    {
      WT_CALL_FUNCTION(event.impl_.function);

      if (event.impl_.handler->context())
        render(*event.impl_.handler);
    }
    catch (std::exception& e)
    {
      LOG_ERROR("Exception in WApplication::notify(): {}", e.what());

#ifdef WT_TARGET_JAVA
      e.printStackTrace();
#endif // WT_TARGET_JAVA
    }
    catch (...)
    {
      LOG_ERROR("Exception in WApplication::notify()");
    }
    co_return;
  }

  Handler* handler = event.impl_.handler;

#ifndef WT_TARGET_JAVA
  if (!app_->initialized_)
  {
    app_->initialized_ = true;
    app_->initialize();
    if (!app_->internalPathValid_)
    {
      //WebResponse *response = handler.response();
      auto context = handler->context();
      if (context && context->responseType() == http::ResponseType::Page)
        context->status(404);
    }
  }
#endif

  if (!handler->context())
    co_return;

  auto context = handler->context();
//  WebRequest& request = *handler.request();
//  WebResponse& response = *handler.response();

  if (WebSession::Handler::instance() != handler)
    // We will want to set these right before doing anything !
    //WebSession::Handler::instance()->setRequest(&request, &response);
    WebSession::Handler::instance()->setRequest(context);

  if (event.impl_.renderOnly) {
    render(*handler);
    co_return;
  }

  auto requestE = context->getParameter("request");

  /*
   * Capture JavaScript error server-side.
   */
  if (requestE == "jserror") {
    auto err = context->getParameter("err");
    if (!err.empty()) {
      app_->handleJavaScriptError(err);
    } else {
      // Forming a custom request with missing err parameter should not crash the server,
      // but our JavaScript should not produce these requests.
      LOG_ERROR("malformed jserror request: missing err parameter");
      app_->handleJavaScriptError("unknown error");
    }
    renderer_.setJSSynced(false);
    render(*handler);
    co_return;
  }

  auto pageIdE = context->getParameter("pageId");
  if (!pageIdE.empty() && pageIdE != std::to_string(renderer_.pageId())) {
    context->res().setContentType("text/javascript; charset=UTF-8");
    context->out() << "{}";
    handler->flushResponse();
    co_return;
  }

  switch (state_) {
  case State::JustCreated:
    render(*handler);

    break;
  case State::ExpectLoad:
  case State::Loaded:
  case State::Suspended:
    /*
     * Excluding resources here ?
     */
    if (((requestE != "resource"))
        && context->responseType() == http::ResponseType::Page) {
      /*
       * Prevent a session fixation attack and a session stealing attack:
       * - user agent has changed: close the session
       * - remote IP address changes: only allowed if the session cookie
       *   is not empty and matches.
       * - prevent attack on ajax sessions:
       *   - use random initial ackUpdateId to prevent attacks on ajax sessions
       *     (in the case somehow the session Id got stolen): this ackUpdateId
       *     is not exposed in a referer
       *   - tie script with unique id to page to prevent attack on a ajax
       *     session: this scriptId is not exposed in a referer
       *
       * Note: this may interfere with the use-case for the undocumented
       *       persistent session configuration option
       */
      if (!env_->agentIsIE()) {
        auto useragent = context->getHeader("User-Agent");
        if (useragent != env_->userAgent()) //str(handler.request()->headerValue("User-Agent")
        {
          LOG_SECURE("change of user-agent not allowed.");
          LOG_INFO("old user agent: {}", env_->userAgent());
          LOG_INFO("new user agent: {}", useragent);
          serveError(403, *handler, "Forbidden");
          co_return;
        }
      }

      auto ca = clientAddress(context->req(), controller_->configuration());// context->req().clientAddress(controller_->configuration());

      if (ca != env_->clientAddress()) {
        bool isInvalid = sessionIdCookie_.empty();

        if (!isInvalid) {
          auto cookie = context->getHeader("Cookie");
          if (cookie.find("Wt" + sessionIdCookie_) == std::string_view::npos)
            isInvalid = true;
        }

        if (isInvalid) {
          LOG_SECURE("change of IP address ({} -> {}) not allowed.", env_->clientAddress(), ca);
          serveError(403, *handler, "Forbidden");
          co_return;
        }
      }
    }

    if (sessionIdCookieChanged_) {
      auto cookie = context->getHeader("Cookie");
      if (cookie.find("Wt" + sessionIdCookie_) == std::string::npos) {
        sessionIdCookie_.clear();
        LOG_INFO("session id cookie not working");
      }

      sessionIdCookieChanged_ = false;
    }

    if (context->responseType() == http::ResponseType::Script) {
      auto sidE = context->getParameter("sid");
      if (sidE != std::to_string(renderer_.scriptId())) {
        throw WException("Script id mismatch");
      }

      if (context->getParameter("skeleton").empty()) {
        if (!env_->ajax()) {
          env_->enableAjax(context);
          app_->enableAjax();
          if (env_->internalPath().length() > 1)
            changeInternalPath(env_->internalPath(), context);
        } else {
          std::string hashE { context->getParameter("_") };
          if (!hashE.empty())
            changeInternalPath(hashE, context);
        }
      }

      render(*handler);
    } else {
      // a normal request to a loaded application
      try {
        if (context->postDataExceeded())
          app_->requestTooLarge().emit(context->postDataExceeded());
      } catch (std::exception& e) {
        LOG_ERROR("Exception in WApplication::requestTooLarge {}", e.what());
        RETHROW(e);
      } catch (...) {
        LOG_ERROR("Exception in WApplication::requestTooLarge");
        throw;
      }

      std::string hashE  {context->getParameter("_")};

      WResource *resource = nullptr;
      if (requestE.empty()) {
        if (!context->pathInfo().empty())
          resource = app_->decodeExposedResource("/path/" + Utils::prepend(std::string(context->pathInfo()), '/'));

        if (!resource && !hashE.empty())
          resource = app_->decodeExposedResource("/path/" + hashE);
      }

      std::string resourceE { context->getParameter("resource") };
      auto signalE = getSignal(context, "");
      auto verE = context->getParameter("ver");

      if (!signalE.empty())
        progressiveBoot_ = false;

      if (resource || (requestE == "resource" && !resourceE.empty())) {
        if (resourceE == "blank") {
          context->type("text/html");
          context->out() <<
            "<html><head><title>bhm</title></head>"
            "<body> </body></html>";
          handler->flushResponse();
        } else {
          if (!resource) {
            unsigned long ver = 0;
            try {
              if (!verE.empty())
                ver = Utils::stoul(verE);
            } catch (std::exception& e) {
              ver = 0;
            }
            resource = app_->decodeExposedResource(resourceE, ver);
           }

          if (resource) {
            try {
              co_await resource->handle(context);
              handler->setRequest(nullptr, nullptr);
            } catch (std::exception& e) {
              LOG_ERROR("Exception while streaming resource {}", e.what());
              RETHROW(e);
            } catch (...) {
              LOG_ERROR("Exception while streaming resource");
              throw;
            }
          } else {
            LOG_ERROR("decodeResource(): resource '{}' not exposed", resourceE);
            context->status(404);
            context->type("text/html");
            context->out() << "<html><body><h1>Page not found.</h1></body></html>";
            handler->flushResponse();
          }
        }
      } else {
        env_->updateUrlScheme(context);

    if (!signalE.empty()) {
	  /*
	   * Check the ackIdE. This is required for a request carrying a signal.
	   */
      auto ackIdE = context->getParameter("ackId");

      bool invalidAckId = env_->ajax() && !context->isWebSocketMessage();

	  WebRenderer::AckState ackState = WebRenderer::CorrectAck;
      if (invalidAckId && !ackIdE.empty()) {
	    try {
          ackState = renderer_.ackUpdate(static_cast<unsigned int>(Utils::stoul(ackIdE)));
	      if (ackState != WebRenderer::BadAck)
            invalidAckId = false;
	    } catch (const std::exception& e) {
	    }
	  }

      if (invalidAckId)
      {
        if (ackIdE.empty())
        {
          LOG_SECURE("missing ackId");
        }
        else {
          LOG_SECURE("invalid ackId");
        }
        serveError(403, *handler, "Forbidden");
        co_return;
	  }

      if (signalE == "poll" &&
	      ackState != WebRenderer::CorrectAck &&
          renderer_.jsSynced())
      {
        LOG_DEBUG("Ignoring poll with incorrect ack -- was rescheduled in browser?");
        handler->flushResponse();
        co_return;
	  }
	  
	  /*
	   * In case we are not using websocket but long polling, the client
	   * aborts the previous poll request to indicate a client-side event.
	   *
	   * So we also discard the previous asyncResponse_ server-side.
	   * We don't do this if we have a websocket request -- it might be
	   * a race between the websocket being established and a poll
	   * request.
	   */
	  if (asyncResponse_) {
	    asyncResponse_->flush();
	    asyncResponse_ = nullptr;
	  }

      if (signalE == "poll") {
#ifdef WT_BOOST_THREADS
	    /*
	     * If we cannot do async I/O, we cannot suspend the current
	     * request and return. Thus we need to block the thread, waiting
	     * for a push update. We wait at most twice as long as the client
	     * will renew this poll connection.
	     */
        if (!WebController::isAsyncSupported() && renderer_.jsSynced())
        {
          updatesPendingEvent_.notify_one();
          if (!updatesPending_)
          {
#ifndef WT_TARGET_JAVA
            updatesPendingEvent_.wait(handler->lock());
#else
            try {
                updatesPendingEvent_.timed_wait
                    (controller_->configuration().serverPushTimeout() * 2);
            } catch (InterruptedException& e) { }
#endif // WT_TARGET_JAVA
          }
          if (!updatesPending_) {
            handler->flushResponse();
            co_return;
          }
        }
#endif // WT_BOOST_THREADS

	    // LOG_DEBUG("poll: " << updatesPending_ << ", " << (asyncResponse_ ? "async" : "no async"));
        if (!updatesPending_ && renderer_.jsSynced())
        {
           /*
	       * If we are ignoring many poll requests (because we are
	       * assuming to have a websocket), we will need to assume
	       * the web socket isn't working properly.
           */
            if (!webSocket_ || (pollRequestsIgnored_ == 2))
            {
                if (webSocket_) {
                    LOG_INFO("discarding broken websocket");
                    webSocket_->flush();
                    webSocket_ = nullptr;
                }

                pollRequestsIgnored_ = 0;
                asyncResponse_ = handler->context(); //handler.response();
                handler->setRequest(nullptr);
                //handler.setRequest(nullptr, nullptr);
            } else {
                ++pollRequestsIgnored_;
                LOG_DEBUG("ignored poll request (#{})", pollRequestsIgnored_);
            }
	    } else
	      pollRequestsIgnored_ = 0;
      }
      else
      {
#ifdef WT_BOOST_THREADS
	    if (!WebController::isAsyncSupported()) {
	      updatesPending_ = false;
	      updatesPendingEvent_.notify_one();
	    }
#endif
	  }

      if (handler->context()) {
        LOG_DEBUG("signal: {}", signalE);

	    /*
	     * Special signal values:
	     * 'poll' : long poll
	     * 'none' : no event, but perhaps a synchronization
	     * 'load' : load invisible content
	     * 'keepAlive' : no event, keep alive
	     */

        try {
          handler->nextSignal = -1;
	      notifySignal(event);
	    } catch (std::exception& e) {
	      LOG_ERROR("error during event handling: {}", e.what());
	      RETHROW(e);
	    } catch (...) {
	      LOG_ERROR("error during event handling");
	      throw;
	    }
	  }
	}

    if (handler->context()
        && handler->context()->responseType() == http::ResponseType::Page
	    && (!env_->ajax() ||
        !controller_->configuration().reloadIsNewSession()))
    {
	  app_->domRoot()->setRendered(false);

      env_->parameters_ = handler->context()->req().getParameters();// handler->context()->getParameterMap();

      if (!hashE.empty())
        changeInternalPath(hashE, context);
      else if (!context->pathInfo().empty()) {
        changeInternalPath(std::string(context->pathInfo()), context);
	  } else
        changeInternalPath("", context);
	}

    if (signalE.empty()) {
	  if (type() == EntryPointType::WidgetSet) {
        LOG_ERROR("bogus request: missing signal, discarding");
        handler->flushResponse();
        co_return;
	  }

	  LOG_INFO("refreshing session");

	  flushBootStyleResponse();

      if (handler->context()) {
        env_->parameters_ = context->req().getParameters();// context->getParameterMap();
        env_->updateHostName(context);
	  }
	    app_->refresh();
	}

    if (handler->context() && !recursiveEventHandler_)
      render(*handler);
      }
    }
  case State::Dead:
    break;
  }
}

void WebSession::changeInternalPath(const std::string& path, WebResponse *response)
{
  if (!app_->internalPathIsChanged_)
    if (!app_->changedInternalPath(path))
      if (response->responseType() == WebResponse::ResponseType::Page)
        response->setStatus(404);
}

void WebSession::changeInternalPath(const std::string &path, http::context *context)
{
  if (!app_->internalPathIsChanged_)
    if (!app_->changedInternalPath(path))
      if (context->responseType() == http::ResponseType::Page)
        context->status(404);
}

EventType WebSession::getEventType(const WEvent& event) const
{  
  if (event.impl_.handler == nullptr) 
    return EventType::Other;
    
  Handler* handler = event.impl_.handler;

#ifndef WT_TARGET_JAVA
  if (event.impl_.function)
    return EventType::Other;
#endif // WT_TARGET_JAVA

  //WebRequest& request = *handler.request();
  auto context = handler->context();

  if (event.impl_.renderOnly || !context)
    return EventType::Other;

  //auto requestE = context->get("request");

  auto pageIdE = context->getParameter("pageId");
  if (!pageIdE.empty() && pageIdE != std::to_string(renderer_.pageId()))
    return EventType::Other;

  switch (state_) {
  case State::ExpectLoad:
  case State::Loaded:
  case State::Suspended:
    if (context->responseType() == http::ResponseType::Script) {
      return EventType::Other;
    } else if (resourceRequest(context)) {
      return EventType::Resource;
    } else {
      auto signalE = getSignal(context, "");

      if (!signalE.empty()) {
        if (signalE == "none" || signalE == "load" ||
            signalE == "hash" || signalE == "poll" ||
            signalE == "keepAlive")
          return EventType::Other;
        else {
          std::vector<unsigned int> signalOrder
            = getSignalProcessingOrder(event);

          unsigned timerSignals = 0;

          for (unsigned i = 0; i < signalOrder.size(); ++i) {
            int signalI = signalOrder[i];
            std::string se = signalI > 0 ? 'e' + std::to_string(signalI) : std::string();
            std::string s { getSignal(context, se) };

            if (s.empty())
              break;
            else if (signalE == "user")
              return EventType::User;
            else {
              EventSignalBase* esb = decodeSignal(s, false);

              if (!esb)
            continue;

              WTimerWidget* t = dynamic_cast<WTimerWidget*>(esb->owner());
              if (t)
                ++timerSignals;
              else
            return EventType::User;
            }
          }

          if (timerSignals)
            return EventType::Timer;
        }
      } else
	return EventType::Other;
    }
  default:
    return EventType::Other;
  }
}

bool WebSession::resourceRequest(const WebRequest& request) const
{
  if (state_ == State::ExpectLoad ||
      state_ == State::Loaded ||
      state_ == State::Suspended)
  {
    const std::string *requestE = request.getParameter("request");
    const std::string *resourceE = request.getParameter("resource");
    if (requestE && *requestE == "resource" && resourceE) {
      return true;
    } else if (!requestE && app_) { // check if resource is deployed on internal path
      if (!request.pathInfo().empty() &&
          app_->decodeExposedResource("/path/" + Utils::prepend(request.pathInfo(), '/')) != nullptr)
        return true;

      const std::string *hashE = request.getParameter("_");
      if (hashE &&
          app_->decodeExposedResource("/path/" + *hashE) != nullptr)
        return true;
    }
  }

  return false;
}

bool WebSession::resourceRequest(Wt::http::context *context) const
{
  if (state_ == State::ExpectLoad ||
      state_ == State::Loaded ||
      state_ == State::Suspended)
  {
    auto requestE = context->getParameter("request");
    auto resourceE = context->getParameter("resource");
    if (requestE == "resource" && !resourceE.empty()) {
      return true;
    } else if (requestE.empty() && app_) { // check if resource is deployed on internal path
      if (!context->pathInfo().empty() /*pathInfo().empty()*/ &&
          app_->decodeExposedResource("/path/" + Utils::prepend(std::string(context->pathInfo()), '/')) != nullptr)
        return true;

      auto hashE = context->getParameter("_");// request.getParameter("_");
      if (/*!hashE.empty() &&*/
          app_->decodeExposedResource("/path/" + std::string(hashE)) != nullptr)
        return true;
    }
  }

  return false;
}

void WebSession::render(Handler& handler)
{
  LOG_DEBUG("render()");
  /*
   * In any case, render() will flush the response, even if an error
   * occurred. Since we are already rendering the response, we can no longer
   * show a nice error message.
   */

  try {
    if (!env_->ajax())
      try {
        checkTimers();
      } catch (std::exception& e) {
        LOG_ERROR("Exception while triggering timers {}", e.what());
        RETHROW(e);
      } catch (...) {
        LOG_ERROR("Exception while triggering timers");
        throw;
      }

    if (app_ && app_->hasQuit())
      kill();

    if (handler.context()) { // a recursive eventloop may remove it in kill()
      updatesPending_ = false;
      serveResponse(handler);
    }

  } catch (std::exception& e) {
    handler.flushResponse();

    RETHROW(e);
  } catch (...) {
    handler.flushResponse();

    throw;
  }
}

void WebSession::serveError(int status, Handler& handler, const std::string& e)
{
  renderer_.serveError(status, handler.context(), e);
  handler.flushResponse();
}

void WebSession::serveResponse(Handler& handler)
{
  auto context = handler.context();

  if (context->responseType() == http::ResponseType::Page)
  {
    pagePathInfo_ = context->pathInfo();
    auto wtdE = context->getParameter("wtd");
    if (!wtdE.empty() && wtdE == sessionId_)
      sessionIdInUrl_ = true;
    else
      sessionIdInUrl_ = false;
  }

  /*
   * If the request is a web socket message, then we should not actually
   * render -- there may be more messages following.
   */
  if (!context->isWebSocketMessage()) {
    /*
     * In any case, flush the style request when we are serving a new
     * page (without Ajax) or the main script (with Ajax).
     */
    if (context->responseType() == http::ResponseType::Script && context->getParameter("skeleton").empty())
    {
#ifndef WT_TARGET_JAVA
      if (bootStyleResponse_) {
        renderer_.serveLinkedCss(bootStyleResponse_);
        flushBootStyleResponse();
      }
#else
      /*
       * Preempt the thread waiting for the bootstyle to be ready.
       * Otherwise there is a reflow as the page already renders
       * without having the CSS (Duh?)
       */
      mutex_.unlock();
      try {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      } catch (InterruptedException& e) { }
      mutex_.lock();
#endif
    }
    renderer_.serveResponse(context);
  }

  handler.flushResponse();
}

void WebSession::propagateFormValues(const WEvent& e, const std::string& se)
{
  //const WebRequest& request = *e.impl_.handler->request();
  auto context = e.impl_.handler->context();

  renderer_.updateFormObjectsList(app_);
  auto& formObjects = renderer_.formObjects();

  auto focus = context->getParameter(se + "focus");
  if (!focus.empty()) {
    int selectionStart = -1, selectionEnd = -1;
    try {
      auto selStart = context->getParameter(se + "selstart");
      if (!selStart.empty())
        selectionStart = Utils::stoi(selStart);

      auto selEnd = context->getParameter(se + "selend");
      if (!selEnd.empty())
        selectionEnd = Utils::stoi(selEnd);
    } catch (std::exception& ee) {
      LOG_ERROR("Could not lexical cast selection range");
    }

    app_->setFocus(std::string(focus), selectionStart, selectionEnd);
  } else
    app_->setFocus(std::string(), -1, -1);

  for (auto i = formObjects.begin(); i != formObjects.end(); ++i)
  {
    std::string formName = i->first;
    WObject *obj = i->second;

    if (!context->postDataExceeded()) {
      WWidget *w = dynamic_cast<WWidget*>(obj);
      // FIXME: reenable isVisible() check once we've fixed all of the regressions
      if (w && (!w->isEnabled()/* || !w->isVisible()*/))
        continue; // Do not update form data of a disabled or invisible widget
      obj->setFormData(getFormData(context, se + formName));
    } else
      obj->setRequestTooLarge(context->postDataExceeded());
  }
}

WObject::FormData WebSession::getFormData(const WebRequest& request, const std::string& name)
{
  std::vector<Http::UploadedFile> files;
  Utils::find(request.uploadedFiles(), name, files);

  return WObject::FormData(request.getParameterValues(name), files);
}

WObject::FormData WebSession::getFormData(http::context *context, const std::string &name)
{
  std::vector<Http::UploadedFile> files;
  Utils::find(context->uploadedFiles(), name, files);

  return WObject::FormData(context->req().getParameterValues(name), files);
}

std::vector<unsigned int> 
WebSession::getSignalProcessingOrder(const WEvent& e) const
{
  // Rush 'onChange' events. Reason: if a user edits a text area and
  // a subsequent click on another element deletes the text area, we
  // have seen situations (at least on firefox) where the clicked event
  // is processed before the changed event, causing the changed event
  // to fail because the event target was deleted.
  WebSession::Handler* handler = e.impl_.handler;
  auto context = handler->context();

  std::vector<unsigned int> highPriority;
  std::vector<unsigned int> normalPriority;

  for (unsigned i = 0;; ++i) {

    std::string se = i > 0 ? 'e' + std::to_string(i) : std::string();
    std::string signalE(getSignal(context, se));
    if (signalE.empty())
      break;
    if (signalE != "user" &&
        signalE != "hash" &&
        signalE != "none" &&
        signalE != "poll" &&
        signalE != "load" &&
        signalE != "keepAlive")
    {
      EventSignalBase *signal = decodeSignal(signalE, true);
      if (!signal) {
        // Signal was not exposed, do nothing
      } else if (signal->name() == WFormWidget::CHANGE_SIGNAL) {
        // compare by pointer in the condition above is ok
        highPriority.push_back(i);
      } else {
        normalPriority.push_back(i);
      }
    } else {
      normalPriority.push_back(i);
    }
  }

  Utils::insert(highPriority, normalPriority);

  return highPriority;
}

void WebSession::notifySignal(const WEvent& e)
{
  WebSession::Handler* handler = e.impl_.handler;
  auto context = handler->context();

  // Reorder signals, as browsers sometimes generate them in a strange order
  if (handler->nextSignal == -1) {
    handler->signalOrder = getSignalProcessingOrder(e);
    handler->nextSignal = 0;
  }

  for (unsigned i = handler->nextSignal; i < handler->signalOrder.size(); ++i) {
    if (!context)
      return;

    //const WebRequest& request = *handler.request();

    int signalI = handler->signalOrder[i];
    std::string se = signalI > 0 ? 'e' + std::to_string(signalI) : std::string();
    std::string signalE { getSignal(context, se) };

    if (signalE.empty())
      return;

    LOG_DEBUG("signal: {}", signalE);

    if (type() != EntryPointType::WidgetSet ||
    (signalE != "none" && signalE != "load"))
      renderer_.setRendered(true);

    if (signalE == "none" || signalE == "load") {
      if (signalE == "load") {
    if (!renderer_.checkResponsePuzzle(context))
	  app_->quit();
	else
	  setLoaded();
      }

      // We will want invisible changes now too.
      renderer_.setVisibleOnly(false);
    } else if (signalE == "keepAlive") {
      // Do nothing
    } else if (signalE != "poll") {
      propagateFormValues(e, se);

      // Save pending changes (e.g. from resource completion)
      // This is needed because we will discard changes from learned
      // signals (see below) and thus need to start with a clean slate
      bool discardStateless = !context->isWebSocketMessage() && i == 0;
      if (discardStateless)
        renderer_.saveChanges();

      handler->nextSignal = i + 1;

      if (signalE == "hash") {
        std::string hashE { context->getParameter(se + "_") };
        if (!hashE.empty()) {
          changeInternalPath(hashE, context);
          app_->doJavaScript(WT_CLASS ".scrollHistory();");
        } else
          changeInternalPath("", context);
      } else {
        for (unsigned k = 0; k < 3; ++k) {
          SignalKind kind = (SignalKind)k;

          if (kind == SignalKind::AutoLearnStateless && context->postDataExceeded())
            break;

          EventSignalBase *s;
          if (signalE == "user") {
            std::string idE { context->getParameter(se + "id") };
            std::string nameE { context->getParameter(se + "name") };

            if (idE.empty() || nameE.empty())
              break;

            s = decodeSignal(idE, nameE, k == 0);
          } else
            s = decodeSignal(signalE, k == 0);

              processSignal(s, se, context, kind);

          if (kind == SignalKind::LearnedStateless && discardStateless)
            renderer_.discardChanges();
        }
      }
    }
  }

  app_->justRemovedSignals().clear();
}

void WebSession::processSignal(EventSignalBase *s, const std::string& se,
                               const WebRequest& request, SignalKind kind)
{
  if (!s)
    return;

  switch (kind) {
  case SignalKind::LearnedStateless:
    s->processLearnedStateless();
    break;
  case SignalKind::AutoLearnStateless:
    s->processAutoLearnStateless(&renderer_);
    break;
  case SignalKind::Dynamic:
    JavaScriptEvent jsEvent;
    jsEvent.get(request, se);
    s->processDynamic(jsEvent);

    // ! handler.request() may be 0 now, if there was a
    // ! recursive call.
    // ! what with other slots triggered after the one that
    // ! did the recursive call ? That's very bad ??
  }
}

void WebSession::processSignal(EventSignalBase *s, const std::string &se,
                               http::context *context, SignalKind kind)
{
  if (!s)
    return;

  switch (kind) {
  case SignalKind::LearnedStateless:
    s->processLearnedStateless();
    break;
  case SignalKind::AutoLearnStateless:
    s->processAutoLearnStateless(&renderer_);
    break;
  case SignalKind::Dynamic:
    JavaScriptEvent jsEvent;
    jsEvent.get(context, se);
    s->processDynamic(jsEvent);

    // ! handler.request() may be 0 now, if there was a
    // ! recursive call.
    // ! what with other slots triggered after the one that
    // ! did the recursive call ? That's very bad ??
  }
}

void WebSession::setPagePathInfo(const std::string& path)
{
  if (!useUglyInternalPaths())
    pagePathInfo_ = path;
}

bool WebSession::useUrlRewriting()
{
  Configuration& conf = controller_->configuration();
  return !(conf.sessionTracking() == Configuration::CookiesURL && env_->supportsCookies());
}

void WebSession::setMultiSessionId(const std::string &multiSessionId)
{
  multiSessionId_ = multiSessionId;
}

#ifndef WT_TARGET_JAVA
void WebSession::generateNewSessionId()
{
  if (!renderer_.isRendered())
    return;

  std::string oldId = sessionId_;
  sessionId_ = controller_->generateNewSessionId(shared_from_this());
  sessionIdChanged_ = true;

  LOG_INFO("new session id for {}", oldId);

  if (!useUrlRewriting()) {
    std::string cookieName = env_->deploymentPath();
    renderer().setCookie(cookieName, sessionId_, WDateTime(), "", "", env_->urlScheme() == "https");
  }

  if (controller_->configuration().sessionIdCookie()) {
    sessionIdCookie_ = WRandom::generateId();
    sessionIdCookieChanged_ = true;
    renderer().setCookie("Wt" + sessionIdCookie_, "1", WDateTime(), "", "", env_->urlScheme() == "https");
  }

  if (controller_->server()->dedicatedSessionProcess()) {
    controller_->server()->updateProcessSessionId(sessionId_);
  }
}
#endif // WT_TARGET_JAVA

#ifndef WT_TARGET_JAVA
void WebSession::setDocRoot(const std::string &docRoot)
{
  docRoot_ = docRoot;
}
#endif // WT_TARGET_JAVA

}
