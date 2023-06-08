/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#include "Wt/WEnvironment.h"

#include "Wt/Utils.h"
#include "Wt/WException.h"
#include "Wt/WLogger.h"
#include "Wt/WSslInfo.h"
#include "Wt/Http/Request.h"
#include "Wt/Json/Parser.h"
#include "Wt/Json/json.hpp"
#include "Wt/WString.h"

#include "Wt/WebController.h"
#include "WebRequest.h"
#include "WebSession.h"
#include "WebUtils.h"
#include "Configuration.h"

#include <boost/algorithm/string.hpp>

#ifndef WT_TARGET_JAVA
#ifdef WT_WITH_SSL
#include <openssl/ssl.h>
#include "SslUtils.h"
#endif //WT_TARGET_JAVA
#endif //WT_WITH_SSL

#define PEM_HEADER "-----BEGIN CERTIFICATE-----"
#define PEM_FOOTER "-----END CERTIFICATE-----"

#define PEM_ESCAPED_HEADER "-----BEGIN%20CERTIFICATE-----"
#define PEM_ESCAPED_FOOTER "-----END%20CERTIFICATE-----%0A"

namespace {
  inline std::string str(const char *s) {
    return s ? std::string(s) : std::string();
  }
}

namespace Wt {

LOGGER("WEnvironment");

WEnvironment::WEnvironment()
  : session_(nullptr),
    doesAjax_(false),
    doesCookies_(false),
    internalPathUsingFragments_(false),
    screenWidth_(-1),
    screenHeight_(-1),
    dpiScale_(1),
    webGLsupported_(false),
    timeZoneOffset_(0)
{ }

WEnvironment::WEnvironment(WebSession *session)
  : session_(session),
    doesAjax_(false),
    doesCookies_(false),
    internalPathUsingFragments_(false),
    screenWidth_(-1),
    screenHeight_(-1),
    dpiScale_(1),
    webGLsupported_(false),
    timeZoneOffset_(0)
{ }

WEnvironment::~WEnvironment()
{ }

void WEnvironment::setInternalPath(const std::string& path)
{
  if (path.empty())
    internalPath_ = path;
  else
    internalPath_ = Utils::prepend(path, '/');
}

const std::string& WEnvironment::deploymentPath() const
{
  if (!publicDeploymentPath_.empty())
    return publicDeploymentPath_;
  else
    return session_->deploymentPath();
}

void WEnvironment::updateHostName(const WebRequest& request)
{
  Configuration& conf = session_->controller()->configuration();
  std::string oldHost = host_;
  host_ = str(request.headerValue("Host"));

  if (conf.behindReverseProxy() ||
      conf.isTrustedProxy(request.remoteAddr())) {
	std::string forwardedHost = str(request.headerValue("X-Forwarded-Host"));

	if (!forwardedHost.empty()) {
	  std::string::size_type i = forwardedHost.rfind(',');
	  if (i == std::string::npos)
		host_ = forwardedHost;
	  else
		host_ = forwardedHost.substr(i+1);
	}

  }
  if(host_.size() == 0) host_ = oldHost;
}

void WEnvironment::updateUrlScheme(const WebRequest& request) 
{
  Configuration& conf = session_->controller()->configuration();

  urlScheme_ = request.urlScheme(conf);
}

void WEnvironment::updateUrlScheme(http::context *context)
{
  Configuration& conf = session_->controller()->configuration();

  if (conf.behindReverseProxy() || conf.isTrustedProxy(context->host())) {
    auto forwardedProto = context->getHeader("X-Forwarded-Proto");
    if (!forwardedProto.empty()) {
      if (auto i = forwardedProto.rfind(','); i == std::string::npos)
        urlScheme_ = forwardedProto;
      else
        urlScheme_ = forwardedProto.substr(i+1);
    }
  }

  urlScheme_ = context->req().scheme();

  //urlScheme_ = context->urlScheme(conf);
}

void WEnvironment::init(const WebRequest& request)
{
  Configuration& conf = session_->controller()->configuration();

  queryString_ = request.queryString();
  //parameters_ = request.getParameterMap();
  host_            = str(request.headerValue("Host"));
  referer_         = str(request.headerValue("Referer"));
  accept_          = str(request.headerValue("Accept"));
  serverSignature_ = str(request.envValue("SERVER_SIGNATURE"));
  serverSoftware_  = str(request.envValue("SERVER_SOFTWARE"));
  serverAdmin_     = str(request.envValue("SERVER_ADMIN"));
  pathInfo_        = request.pathInfo();

#ifndef WT_TARGET_JAVA
  if(!str(request.headerValue("Redirect-Secret")).empty())
	session_->controller()->redirectSecret_ = str(request.headerValue("Redirect-Secret"));

  sslInfo_ = request.sslInfo(conf);
#endif

  setUserAgent(str(request.headerValue("User-Agent")));
  updateUrlScheme(request);

  LOG_INFO("UserAgent: {}", userAgent_);

  /*
   * If behind a reverse proxy, use external host, schema as communicated using 'X-Forwarded'
   * headers.
   */
  if (conf.behindReverseProxy() ||
      conf.isTrustedProxy(request.remoteAddr())) {
    std::string forwardedHost = str(request.headerValue("X-Forwarded-Host"));

    if (!forwardedHost.empty()) {
      std::string::size_type i = forwardedHost.rfind(',');
      if (i == std::string::npos)
	host_ = forwardedHost;
      else
	host_ = forwardedHost.substr(i+1);
    }
  }



  if (host_.empty()) {
    /*
     * HTTP 1.0 doesn't require it: guess from config
     */
    host_ = request.serverName();
    if (!request.serverPort().empty())
      host_ += ":" + request.serverPort();
  }

  clientAddress_ = request.clientAddress(conf);

  const char *cookie = request.headerValue("Cookie");
  doesCookies_ = cookie;

  if (cookie)
    parseCookies(cookie, cookies_);

  locale_ = request.parseLocale();
}

void WEnvironment::updateHostName(http::context *context)
{
  Configuration& conf = session_->controller()->configuration();
  std::string oldHost = host_;
  host_ = context->getHeader("Host");

  if (conf.behindReverseProxy() ||
      conf.isTrustedProxy(context->host())) {
    auto forwardedHost = context->getHeader("X-Forwarded-Host");

    if (!forwardedHost.empty()) {
      auto i = forwardedHost.rfind(',');
      if (i == std::string_view::npos)
        host_ = forwardedHost;
      else
        host_ = forwardedHost.substr(i+1);
    }

  }
  if(host_.size() == 0) host_ = oldHost;
}

void WEnvironment::enableAjax(Wt::http::context *context)
{
  doesAjax_ = true;
  session_->controller()->newAjaxSession();

  doesCookies_ = !context->getHeader("Cookie").empty();

  if (context->getParameter("htmlHistory").empty())
    internalPathUsingFragments_ = true;

  auto scaleE = context->getParameter("scale");

  try {
    dpiScale_ = !scaleE.empty() ? Utils::stod(scaleE) : 1;
  } catch (std::exception& e) {
    dpiScale_ = 1;
  }

  auto webGLE = context->getParameter("webGL");

  webGLsupported_ = webGLE == "true";

  auto tzE = context->getParameter("tz");

  try {
    timeZoneOffset_ = std::chrono::minutes(!tzE.empty() ? Utils::stoi(tzE) : 0);
  } catch (std::exception& e) {
  }

  auto tzSE = context->getParameter("tzS");

  timeZoneName_ = tzSE;

  auto hashE = context->getParameter("_");

  // the internal path, when present as an anchor (#), is only
  // conveyed in the second request
  if (!hashE.empty())
    setInternalPath(std::string(hashE));

  auto deployPathE = context->getParameter("deployPath");
  if (!deployPathE.empty()) {
    publicDeploymentPath_ = deployPathE;
    std::size_t s = publicDeploymentPath_.find('/');
    if (s != 0)
      publicDeploymentPath_.clear(); // looks invalid
  }

  auto scrWE = context->getParameter("scrW");
  if (!scrWE.empty()) {
    try {
      screenWidth_ = Utils::stoi(scrWE);
    } catch (std::exception &e) {
    }
  }
  auto scrHE = context->getParameter("scrH");
  if (!scrHE.empty()) {
    try {
      screenHeight_ = Utils::stoi(scrHE);
    } catch (std::exception &e) {
    }
  }
}


void WEnvironment::enableAjax(const WebRequest& request)
{
  doesAjax_ = true;
  session_->controller()->newAjaxSession();

  doesCookies_ = request.headerValue("Cookie") != nullptr;

  if (!request.getParameter("htmlHistory"))
    internalPathUsingFragments_ = true;

  const std::string *scaleE = request.getParameter("scale");

  try {
    dpiScale_ = scaleE ? Utils::stod(*scaleE) : 1;
  } catch (std::exception& e) {
    dpiScale_ = 1;
  }

  const std::string *webGLE = request.getParameter("webGL");

  webGLsupported_ = webGLE ? (*webGLE == "true") : false;

  const std::string *tzE = request.getParameter("tz");

  try {
    timeZoneOffset_ = std::chrono::minutes(tzE ? Utils::stoi(*tzE) : 0);
  } catch (std::exception& e) {
  }

  const std::string *tzSE = request.getParameter("tzS");

  timeZoneName_ = tzSE ? *tzSE : std::string("");

  const std::string *hashE = request.getParameter("_");

  // the internal path, when present as an anchor (#), is only
  // conveyed in the second request
  if (hashE)
    setInternalPath(*hashE);

  const std::string *deployPathE = request.getParameter("deployPath");
  if (deployPathE) {
    publicDeploymentPath_ = *deployPathE;
    std::size_t s = publicDeploymentPath_.find('/');
    if (s != 0)
      publicDeploymentPath_.clear(); // looks invalid
  }

  const std::string *scrWE = request.getParameter("scrW");
  if (scrWE) {
    try {
      screenWidth_ = Utils::stoi(*scrWE);
    } catch (std::exception &e) {
    }
  }
  const std::string *scrHE = request.getParameter("scrH");
  if (scrHE) {
    try {
      screenHeight_ = Utils::stoi(*scrHE);
    } catch (std::exception &e) {
    }
  }
}

std::string_view clientaddress(http::request& request, const Wt::Configuration &conf)
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
//#ifdef HTTP_WITH_SSL
std::unique_ptr<Wt::WSslInfo> sslInfoFromJson(Wt::http::context* context)
{
  auto ssl_client_certificates = context->getHeader("X-Wt-Ssl-Client-Certificates");

  if (ssl_client_certificates.empty())
    return nullptr;

  Wt::Json::Object obj;
  Wt::Json::ParseError error;
  if (!Wt::Json::parse(Wt::Utils::base64Decode(ssl_client_certificates), obj, error)) {
    LOG_ERROR("error while parsing client certificates");
    return nullptr;
  }

  std::string clientCertificatePem = obj["client-certificate"].get_string("");

  X509 *cert = Wt::Ssl::readFromPem(clientCertificatePem);

  if (cert) {
    Wt::WSslCertificate clientCert = Wt::Ssl::x509ToWSslCertificate(cert);
    X509_free(cert);

    const Wt::Json::Array &arr = obj["client-pem-certification-chain"].as_array();

    std::vector<Wt::WSslCertificate> clientCertChain;

    for (const auto &cert : arr) {
      clientCertChain.push_back(Wt::Ssl::x509ToWSslCertificate(Wt::Ssl::readFromPem(cert.get_string(""))));
    }

    Wt::ValidationState state = static_cast<Wt::ValidationState>(static_cast<int>(obj["client-verification-result-state"].get_int64(0)));
    Wt::WString message = obj["client-verification-result-message"].get_string("");

    return std::make_unique<Wt::WSslInfo>(clientCert,
                                          clientCertChain,
                                          Wt::WValidator::Result(state, message));
  }

  return nullptr;
}
//#endif // HTTP_WITH_SSL

std::unique_ptr<Wt::WSslInfo> sslInfoFromHeaders(Wt::http::context* context)
{
  auto client_verify = context->getHeader("X-SSL-Client-Verify");
  auto client_s_dn = context->getHeader("X-SSL-Client-S-DN");
  auto client_i_dn = context->getHeader("X-SSL-Client-I-DN");
  auto validity_start = context->getHeader("X-SSL-Client-V-Start");
  auto validity_end = context->getHeader("X-SSL-Client-V-End");
  auto client_cert = context->getHeader("X-SSL-Client-Cert");

  if (client_verify.empty())
    return nullptr;

  if (boost::iequals(client_verify, "NONE"))
    return nullptr;

  enum Verify {
      SUCCESS,
      FAILED,
      GENEROUS
  };

  const char *failedReason = nullptr;

  Verify v = FAILED;
  if (boost::iequals(client_verify, "SUCCESS"))
    v = SUCCESS;
  else if (boost::iequals(client_verify, "GENEROUS"))
    v = GENEROUS;
  else if (boost::istarts_with(client_verify, "FAILED:")) {
    v = FAILED;
    failedReason = client_verify.data() + sizeof("FAILED");
  } else
    return nullptr;

  std::string clientCertStr;
  if (!client_cert.empty()) {
    clientCertStr = client_cert;
    boost::trim(clientCertStr);
    if (boost::starts_with(clientCertStr, PEM_HEADER)) {
      const std::size_t start = sizeof(PEM_HEADER) - 1;
      const std::size_t end = clientCertStr.find(PEM_FOOTER);
      if (end != std::string::npos) {
        for (std::size_t i = start; i < end; ++i) {
            if (clientCertStr[i] == ' ')
                clientCertStr[i] = '\n';
        }
      } else {
        clientCertStr.clear();
      }
    } else if (boost::starts_with(clientCertStr, PEM_ESCAPED_HEADER) &&
               boost::ends_with(clientCertStr, PEM_ESCAPED_FOOTER)) {
      clientCertStr = Wt::Utils::urlDecode(clientCertStr);
    } else {
      clientCertStr.clear();
    }
  }

#ifdef WT_WITH_SSL
  if (!clientCertStr.empty()) {
    // try parse cert, use cert for all other info
    X509 *cert = Wt::Ssl::readFromPem(clientCertStr);

    if (cert) {
      Wt::WSslCertificate clientCert = Wt::Ssl::x509ToWSslCertificate(cert);
      return std::make_unique<Wt::WSslInfo>(clientCert,
                                            std::vector<Wt::WSslCertificate>(),
                                            Wt::WValidator::Result(v == SUCCESS ? Wt::ValidationState::Valid : Wt::ValidationState::Invalid,
                                                                   failedReason ? Wt::utf8(failedReason) : Wt::WString::Empty));
    }
  }
#endif // WT_WITH_SSL

  if (!client_s_dn.empty() &&
      !client_i_dn.empty() &&
      !validity_start.empty() &&
      !validity_end.empty())
  {
    std::vector<Wt::WSslCertificate::DnAttribute> subjectDn = Wt::WSslCertificate::dnFromString(client_s_dn);
    std::vector<Wt::WSslCertificate::DnAttribute> issuerDn = Wt::WSslCertificate::dnFromString(client_i_dn);

    const Wt::WString validityFormat = Wt::utf8("MMM dd hh:mm:ss yyyy 'GMT'");
    Wt::WDateTime validityStart = Wt::WDateTime::fromString(Wt::utf8(validity_start), validityFormat);
    Wt::WDateTime validityEnd = Wt::WDateTime::fromString(Wt::utf8(validity_end), validityFormat);

    Wt::WSslCertificate clientCert(subjectDn,
                                   issuerDn,
                                   validityStart,
                                   validityEnd,
                                   clientCertStr);
    return std::make_unique<Wt::WSslInfo>(clientCert,
                                          std::vector<Wt::WSslCertificate>(),
                                          Wt::WValidator::Result(v == SUCCESS ? Wt::ValidationState::Valid : Wt::ValidationState::Invalid,
                                                                 failedReason ? Wt::utf8(failedReason) : Wt::WString::Empty));
  }

  return nullptr;
}

std::unique_ptr<Wt::WSslInfo> getSslInfo(http::context *context, const Wt::Configuration &conf)
{
  std::unique_ptr<Wt::WSslInfo> result;// = context->sslInfo();
  if (conf.behindReverseProxy() ||
      conf.isTrustedProxy(context->host())) {
#ifdef WT_WITH_SSL
    if (!result)
      result = sslInfoFromJson(context);
#endif // WT_WITH_SSL
    if (!result)
      result = sslInfoFromHeaders(context);
  }
  return result;
}

#warning "need to be tested !"
void WEnvironment::init(http::context *context)
{
  Configuration& conf = session_->controller()->configuration();

  queryString_ = context->querystring();
  parameters_ = context->req().getParameters();
  host_            = context->getHeader("Host");
  referer_         = context->getHeader("Referer");
  accept_          = context->getHeader("Accept");
  serverSignature_ = context->req().envValue("SERVER_SIGNATURE");
  serverSoftware_  = context->req().envValue("SERVER_SOFTWARE");
  serverAdmin_     = context->req().envValue("SERVER_ADMIN");
  pathInfo_        = context->pathInfo();

#ifndef WT_TARGET_JAVA
  if(auto secret = context->getHeader("Redirect-Secret"); !secret.empty())
    session_->controller()->redirectSecret_ = secret;

  sslInfo_ = getSslInfo(context, conf);
#endif

  setUserAgent(context->getHeader("User-Agent"));
  updateUrlScheme(context);

  LOG_INFO("UserAgent: {}", userAgent_);

  /*
   * If behind a reverse proxy, use external host, schema as communicated using 'X-Forwarded'
   * headers.
   */
  if (conf.behindReverseProxy() || conf.isTrustedProxy(context->req().host()))
  {
    auto forwardedHost = context->getHeader("X-Forwarded-Host");

    if (!forwardedHost.empty()) {
      std::string::size_type i = forwardedHost.rfind(',');
      if (i == std::string::npos)
        host_ = forwardedHost;
      else
        host_ = forwardedHost.substr(i+1);
    }
  }



  if (host_.empty()) {
    /*
     * HTTP 1.0 doesn't require it: guess from config
     */
//    host_ = context->hostname();
//    if (!context->port().empty())
      host_ = context->host();
  }

  clientAddress_ = clientaddress(context->req(), conf);

  std::string cookie { context->getHeader("Cookie") };
  doesCookies_ = !cookie.empty();

  if (!cookie.empty())
    parseCookies(cookie, cookies_);

  locale_ = Wt::WLocale(context->req().parsePreferredAcceptValue()); ;
}

void WEnvironment::setUserAgent(std::string_view userAgent)
{
  userAgent_ = userAgent;

  Configuration& conf = session_->controller()->configuration();

  agent_ = UserAgent::Unknown;

  /* detecting MSIE is as messy as their browser */
  if (userAgent_.find("Trident/4.0") != std::string::npos) {
    agent_ = UserAgent::IE8; return;
  } if (userAgent_.find("Trident/5.0") != std::string::npos) {
    agent_ = UserAgent::IE9; return;
  } else if (userAgent_.find("Trident/6.0") != std::string::npos) {
    agent_ = UserAgent::IE10; return;
  } else if (userAgent_.find("Trident/") != std::string::npos) {
    agent_ = UserAgent::IE11; return;
  } else if (userAgent_.find("MSIE 2.") != std::string::npos
      || userAgent_.find("MSIE 3.") != std::string::npos
      || userAgent_.find("MSIE 4.") != std::string::npos
      || userAgent_.find("MSIE 5.") != std::string::npos
      || userAgent_.find("IEMobile") != std::string::npos)
    agent_ = UserAgent::IEMobile;
  else if (userAgent_.find("MSIE 6.") != std::string::npos)
    agent_ = UserAgent::IE6;
  else if (userAgent_.find("MSIE 7.") != std::string::npos)
    agent_ = UserAgent::IE7;
  else if (userAgent_.find("MSIE 8.") != std::string::npos)
    agent_ = UserAgent::IE8;
  else if (userAgent_.find("MSIE 9.") != std::string::npos)
    agent_ = UserAgent::IE9;
  else if (userAgent_.find("MSIE") != std::string::npos)
    agent_ = UserAgent::IE10;

  if (userAgent_.find("Opera") != std::string::npos) {
    agent_ = UserAgent::Opera;

    std::size_t t = userAgent_.find("Version/");
    if (t != std::string::npos) {
      std::string vs = userAgent_.substr(t + 8);
      t = vs.find(' ');
      if (t != std::string::npos)
	vs = vs.substr(0, t);
      try {
	double v = Utils::stod(vs);
	if (v >= 10)
	  agent_ = UserAgent::Opera10;
      } catch (std::exception& e) { }
    }
  }

  if (userAgent_.find("Chrome") != std::string::npos) {
    if (userAgent_.find("Android") != std::string::npos)
      agent_ = UserAgent::MobileWebKitAndroid;
    else if (userAgent_.find("Chrome/0.") != std::string::npos)
      agent_ = UserAgent::Chrome0;
    else if (userAgent_.find("Chrome/1.") != std::string::npos)
      agent_ = UserAgent::Chrome1;
    else if (userAgent_.find("Chrome/2.") != std::string::npos)
      agent_ = UserAgent::Chrome2;
    else if (userAgent_.find("Chrome/3.") != std::string::npos)
      agent_ = UserAgent::Chrome3;
    else if (userAgent_.find("Chrome/4.") != std::string::npos)
      agent_ = UserAgent::Chrome4;
    else
      agent_ = UserAgent::Chrome5;
  } else if (userAgent_.find("Safari") != std::string::npos) {
    if (userAgent_.find("iPhone") != std::string::npos
	|| userAgent_.find("iPad") != std::string::npos) {
      agent_ = UserAgent::MobileWebKitiPhone;
    } else if (userAgent_.find("Android") != std::string::npos) {
      agent_ = UserAgent::MobileWebKitAndroid;
    } else if (userAgent_.find("Mobile") != std::string::npos) {
      agent_ = UserAgent::MobileWebKit;
    } else if (userAgent_.find("Version") == std::string::npos) {
      if (userAgent_.find("Arora") != std::string::npos)
	agent_ = UserAgent::Arora;
      else
	agent_ = UserAgent::Safari;
    } else if (userAgent_.find("Version/3") != std::string::npos)
      agent_ = UserAgent::Safari3;
    else
      agent_ = UserAgent::Safari4;
  } else if (userAgent_.find("WebKit") != std::string::npos) {
    if (userAgent_.find("iPhone") != std::string::npos)
      agent_ = UserAgent::MobileWebKitiPhone;
    else
      agent_ = UserAgent::WebKit;
  } else if (userAgent_.find("Konqueror") != std::string::npos)
    agent_ = UserAgent::Konqueror;
  else if (userAgent_.find("Gecko") != std::string::npos)
    agent_ = UserAgent::Gecko;

  if (userAgent_.find("Firefox") != std::string::npos) {
    if (userAgent_.find("Firefox/0.") != std::string::npos)
      agent_ = UserAgent::Firefox;
    else if (userAgent_.find("Firefox/1.") != std::string::npos)
      agent_ = UserAgent::Firefox;
    else if (userAgent_.find("Firefox/2.") != std::string::npos)
      agent_ = UserAgent::Firefox;
    else {
      if (userAgent_.find("Firefox/3.0") != std::string::npos)
	agent_ = UserAgent::Firefox3_0;
      else if (userAgent_.find("Firefox/3.1") != std::string::npos)
	agent_ = UserAgent::Firefox3_1;
      else if (userAgent_.find("Firefox/3.1b") != std::string::npos)
	agent_ = UserAgent::Firefox3_1b;
      else if (userAgent_.find("Firefox/3.5") != std::string::npos)
	agent_ = UserAgent::Firefox3_5;
      else if (userAgent_.find("Firefox/3.6") != std::string::npos)
	agent_ = UserAgent::Firefox3_6;
      else if (userAgent_.find("Firefox/4.") != std::string::npos)
	agent_ = UserAgent::Firefox4_0;
      else
	agent_ = UserAgent::Firefox5_0;
    }
  }

  if (userAgent_.find("Edge/") != std::string::npos) {
    agent_ = UserAgent::Edge;
  }

  if (conf.agentIsBot(userAgent_))
    agent_ = UserAgent::BotAgent;
}

bool WEnvironment::agentSupportsAjax() const
{
  Configuration& conf = session_->controller()->configuration();

  return conf.agentSupportsAjax(userAgent_);
}

bool WEnvironment::supportsCss3Animations() const
{
  return ((agentIsGecko() && 
	   static_cast<unsigned int>(agent_) >= 
	   static_cast<unsigned int>(UserAgent::Firefox5_0)) ||
	  (agentIsIE() && 
	   static_cast<unsigned int>(agent_) >= 
	   static_cast<unsigned int>(UserAgent::IE10)) ||
	  agentIsWebKit());
}

std::string WEnvironment::libraryVersion()
{
  return WT_VERSION_STR;
}

#ifndef WT_TARGET_JAVA
void WEnvironment::libraryVersion(int& series, int& major, int& minor) const
{
  series = WT_SERIES;
  major = WT_MAJOR;
  minor = WT_MINOR;
}
#endif //WT_TARGET_JAVA

static http::ParameterValues emptyValues_;

const http::ParameterValues&
WEnvironment::getParameterValues(const std::string& name) const
{
  auto i = parameters_.find(name);

  if (i != parameters_.end())
    return i->second;
  else
    return emptyValues_;
}

const std::string *WEnvironment::getParameter(const std::string& name) const
{
  const auto& values = getParameterValues(name);
  if (!Utils::isEmpty(values))
    return &values[0];
  else
    return nullptr;
}

const std::string *WEnvironment::getCookie(const std::string& cookieName)
  const
{
  CookieMap::const_iterator i = cookies_.find(cookieName);

  if (i == cookies_.end())
    return nullptr;
  else
    return &i->second;
}

const std::string WEnvironment::headerValue(const std::string& name) const
{
  return session_->getCgiHeader(name);
}

std::string WEnvironment::getCgiValue(const std::string& varName) const
{
  if (varName == "QUERY_STRING")
    return queryString_;
  else
    return session_->getCgiValue(varName);
}

WServer *WEnvironment::server() const
{
#ifndef WT_TARGET_JAVA
  return session_->controller()->server();
#else
  return session_->controller();
#endif // WT_TARGET_JAVA
}

bool WEnvironment::isTest() const
{
  return false;
}

void WEnvironment::parseCookies(const std::string& cookie,
				std::unordered_map<std::string, std::string>& result)
{
  // Cookie parsing strategy:
  // - First, split the string on cookie separators (-> name-value pair).
  //   ';' is cookie separator. ',' is not a cookie separator (as in PHP)
  // - Then, split the name-value pairs on the first '='
  // - URL decoding/encoding
  // - Trim the name, trim the value
  // - If a name-value pair does not contain an '=', the name-value pair
  //   was the name of the cookie and the value is empty

  std::vector<std::string> list;
  boost::split(list, cookie, boost::is_any_of(";"));
  for (unsigned int i = 0; i < list.size(); ++i) {
    std::string::size_type e = list[i].find('=');
    if (e == std::string::npos)
      continue;
    std::string cookieName = list[i].substr(0, e);
    std::string cookieValue =
      (e != std::string::npos && list[i].size() > e + 1) ?
      list[i].substr(e + 1) : "";

    boost::trim(cookieName);
    boost::trim(cookieValue);

    cookieName = Wt::Utils::urlDecode(cookieName);
    cookieValue = Wt::Utils::urlDecode(cookieValue);
    if (cookieName != "")
      result[cookieName] = cookieValue;
  }
}
Signal<awaitable<void>(WDialog *)>& WEnvironment::dialogExecuted() const
{
  throw WException("Internal error");
}

Signal<awaitable<void>(WPopupMenu *)>& WEnvironment::popupExecuted() const
{
  throw WException("Internal error");
}

}
