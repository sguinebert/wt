/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#include <string>

#include "Wt/WResource.h"
#include "Wt/WApplication.h"
#include "Wt/Http/Request.h"
#include "Wt/Http/Response.h"

#include "Wt/WebController.h"
#include "WebRequest.h"
#include "WebSession.h"
#include "WebUtils.h"

#include <memory>

namespace Wt
{

  LOGGER("WResource");

  /*
 * Resource locking strategy:
 *
 * A resource is reentrant: multiple calls to handleRequest can happen
 * simultaneously.
 *
 * The mutex_ protects:
 *  - beingDeleted_: indicates that the resource wants to be deleted,
 *    and thus should no longer be used (by continuations)
 *  - useCount_: number of requests currently being handled
 *  - continuations_: the list of continuations
 */

  WResource::UseLock::UseLock()
      : resource_(nullptr)
  {
  }

  bool WResource::UseLock::use(WResource *resource)
  {
#ifdef WT_THREADED
    if (resource && !resource->beingDeleted_.load(std::memory_order_relaxed))
    {
      resource_ = resource;
      resource_->useCount_.fetch_add(1, std::memory_order_relaxed);//  ++resource_->useCount_;

      return true;
    }
    else
      return false;
#else
    return true;
#endif
  }

  WResource::UseLock::~UseLock()
  {
#ifdef WT_THREADED
    if (resource_)
    {
      //std::unique_lock<std::recursive_mutex> lock(*resource_->mutex_);
       //--resource_->useCount_;
      if (resource_->useCount_.fetch_sub(1, std::memory_order_relaxed) == 0)
        resource_->useDone_.notify_one();
    }
#endif
  }

  WResource::WResource()
      : trackUploadProgress_(false),
        takesUpdateLock_(false),
        invalidAfterChanged_(false),
        dispositionType_(ContentDisposition::None),
        version_(0),
        app_(nullptr)
  {
#ifdef WT_THREADED
    mutex_.reset(new std::mutex());
//    beingDeleted_ = false;
//    useCount_ = 0;
#endif // WT_THREADED
  }

  void WResource::beingDeleted()
  {
    //std::vector<Http::ResponseContinuationPtr> cs;

    LOG_DEBUG("beingDeleted()");
    beingDeleted_.store(true, std::memory_order_relaxed);

    cancel_signal_.emit(asio::cancellation_type::total); //what if handler called after ?

    {
#ifdef WT_THREADED
      std::unique_lock<std::mutex> lock(*mutex_);
      //std::ranges::for_each(vec, [](http::response& res) { res.haveMoreData_(); });
//      for(auto& css : cs_) { //deprecated ?
//        css->cancel();
//      }

      while (useCount_ > 0)
        useDone_.wait(lock);
#endif // WT_THREADED

//      cs = continuations_;

//      continuations_.clear();
    }

//    for (unsigned i = 0; i < cs.size(); ++i)
//      cs[i]->cancel(true);
  }

  WResource::~WResource()
  {
    beingDeleted();

    WApplication *app = WApplication::instance();
    if (app)
    {
      app->removeExposedResource(this);
      if (trackUploadProgress_)
        WebSession::instance()->controller()->removeUploadProgressUrl(url());
    }
  }

  void WResource::setUploadProgress(bool enabled)
  {
    if (trackUploadProgress_ != enabled)
    {
      trackUploadProgress_ = enabled;

      WebController *c = WebSession::instance()->controller();
      if (enabled)
        c->addUploadProgressUrl(url());
      else
        c->removeUploadProgressUrl(url());
    }
  }

  void WResource::haveMoreData()
  {
    if(beingDeleted_.load(std::memory_order_acquire))
      return;

    std::vector<std::shared_ptr<http::Continuation>> cs;
    {
#ifdef WT_THREADED
      std::unique_lock<std::mutex> lock(*mutex_);
      cs = cs_;
      cs_.clear();
#endif // WT_THREADED
    }

    for(auto& css : cs){
      css->haveMoreData();
    }
  }

//  void WResource::doContinue(Http::ResponseContinuationPtr continuation)
//  {
//    WebResponse *webResponse = continuation->response();
//    WebRequest *webRequest = webResponse;

//    try
//    {
//      handle(webRequest, webResponse, continuation);
//    }
//    catch (std::exception &e)
//    {
//      LOG_ERROR("exception while handling resource continuation: {}", e.what());
//    }
//    catch (...)
//    {
//      LOG_ERROR("exception while handling resource continuation");
//    }
//  }

//  void WResource::removeContinuation(Http::ResponseContinuationPtr continuation)
//  {
//#ifdef WT_THREADED
//    std::unique_lock<std::mutex> lock(*mutex_);
//#endif
//    Utils::erase(continuations_, continuation);
//  }

//  Http::ResponseContinuationPtr
//  WResource::addContinuation(Http::ResponseContinuation *c)
//  {
//    Http::ResponseContinuationPtr result(c);

//#ifdef WT_THREADED
//    std::unique_lock<std::mutex> lock(*mutex_);
//#endif
//    continuations_.push_back(result);

//    return result;
//  }

//  void WResource::handle(WebRequest *webRequest, WebResponse *webResponse,
//                         Http::ResponseContinuationPtr continuation)
//  {
    /*
   * If we are a new request for a dynamic resource, then we will have
   * the session lock at this point and thus the resource is protected
   * against deletion.
   *
   * If we come from a continuation, then the continuation increased the
   * use count and we are thus protected against deletion.
   */
//    WebSession::Handler *handler = WebSession::Handler::instance();
//    UseLock useLock;

//#ifdef WT_THREADED
//    std::unique_ptr<Wt::WApplication::UpdateLock> updateLock;
//    if (takesUpdateLock() && continuation && app_)
//    {
//      updateLock.reset(new Wt::WApplication::UpdateLock(app_));
//      if (!*updateLock)
//      {
//        return;
//      }
//    }

//    if (handler && !continuation)
//    {
//      std::unique_lock<std::recursive_mutex> lock(*mutex_);

//      if (!useLock.use(this))
//        return;

//      if (!takesUpdateLock() &&
//          handler->haveLock() &&
//          handler->lockOwner() == std::this_thread::get_id())
//      {
//        handler->unlock();
//      }
//    }
//#endif // WT_THREADED

//    // if (!handler) {
//    //   WLocale locale = webRequest->parseLocale();
//    //   WLocale::setCurrentLocale(locale);
//    // }

//    Http::Request request(*webRequest, continuation.get());
//    Http::Response response(this, webResponse, continuation);

//    if (!continuation)
//      response.setStatus(200);

//    handleRequest(request, response);

//#ifdef WT_THREADED
//    updateLock.reset();
//#endif // WT_THREADED

//    if (!response.continuation_ || !response.continuation_->resource_)
//    {
//      if (response.continuation_)
//        removeContinuation(response.continuation_);

//      response.out(); // trigger committing the headers if still necessary

//      webResponse->flush(WebResponse::ResponseState::ResponseDone);
//    }
//    else
//    {
//      webResponse->flush(WebResponse::ResponseState::ResponseFlush,
//                         std::bind(&Http::ResponseContinuation::readyToContinue,
//                                   response.continuation_,
//                                   std::placeholders::_1));
//    }
//  }

  awaitable<void> WResource::handle(Wt::http::context *ctx)
  {
  /*
   * If we are a new request for a dynamic resource, then we will have
   * the session lock at this point and thus the resource is protected
   * against deletion.
   *
   * If we come from a continuation, then the continuation increased the
   * use count and we are thus protected against deletion.
   */
    WebSession::Handler *handler = WebSession::Handler::instance();
    UseLock useLock;

#ifdef WT_THREADED
    /* NO continuation recursive loop with coroutine */
//    std::unique_ptr<Wt::WApplication::UpdateLock> updateLock;
    /* NO continuation recursive loop with coroutine */
//    if (takesUpdateLock() && continuation && app_) /* DEPRECATED BY COROUTINE (since we can flush inside a simple loop / continuation is deprecated)*/
//    {
//      updateLock.reset(new Wt::WApplication::UpdateLock(app_));
//      if (!*updateLock)
//      {
//        co_return;
//      }
//    }

    if (handler /*&& !continuation*/)
    {
      //1./ recursive is deprecated since continuation is not needed anymore 2/. not needed if resource_->useCount_ is atomic
      //std::unique_lock<std::recursive_mutex> lock(*mutex_);
      if (!useLock.use(this))
        co_return;

      if (!takesUpdateLock() && //no lock except for webcontroller->shutdown() : suspended coroutines are not a problem
          handler->haveLock() &&
          handler->lockOwner() == std::this_thread::get_id())
      {
        handler->unlock();
      }
    }
#endif // WT_THREADED

    /* let the user decide if he needs local or not */
    // if (!handler) {
    //   WLocale locale = webRequest->parseLocale();
    //   WLocale::setCurrentLocale(locale);
    // }

    ctx->status(200);

//    co_await handleRequest(*ctx);
    co_await handleRequest(ctx->req(), ctx->res());

    /* NO continuation recursive loop with coroutine */
//#ifdef WT_THREADED
//    updateLock.reset();
//#endif // WT_THREADED

    ctx->flush();

    /* DEPRECATED with coroutines */
//    if (!ctx->continuation_ || !ctx->continuation_->resource_)
//    {
//      if (ctx->continuation_)
//        removeContinuation(ctx->continuation_);

      //response.out(); // trigger committing the headers if still necessary

      //webResponse->flush(WebResponse::ResponseState::ResponseDone);

//    }
//    else
//    {
      //ctx->flush();
//      webResponse->flush(WebResponse::ResponseState::ResponseFlush,
//                         std::bind(&Http::ResponseContinuation::readyToContinue,
//                                   response.continuation_,
//                                   std::placeholders::_1));
//    }
    co_return;
  }

  void WResource::setLocale(Wt::http::request &request)
  {
    WLocale locale(request.parsePreferredAcceptValue());
    WLocale::setCurrentLocale(locale);
  }

  void WResource::suggestFileName(const WString &name,
                                  ContentDisposition dispositionType)
  {
    suggestedFileName_ = name;
    dispositionType_ = dispositionType;

    currentUrl_.clear();
  }

  void WResource::setInternalPath(const std::string &path)
  {
    WApplication *app = WApplication::instance();

    bool wasExposed = app && app->removeExposedResource(this);

    internalPath_ = path;
    currentUrl_.clear();

    if (wasExposed)
      app->addExposedResource(this);
  }

  void WResource::setDispositionType(ContentDisposition dispositionType)
  {
    dispositionType_ = dispositionType;
  }

  awaitable<void> WResource::setChanged()
  {
    generateUrl();

    co_await dataChanged_.emit();
  }

  void WResource::setInvalidAfterChanged(bool enabled)
  {
    invalidAfterChanged_ = enabled;
  }

  const std::string &WResource::url() const
  {
    if (currentUrl_.empty())
      (const_cast<WResource *>(this))->generateUrl();

    return currentUrl_;
  }

  const std::string &WResource::generateUrl()
  {
    WApplication *app = WApplication::instance();

    if (app)
    {
      WebController *c = nullptr;
      if (trackUploadProgress_)
        c = WebSession::instance()->controller();

      if (c && !currentUrl_.empty())
        c->removeUploadProgressUrl(currentUrl_);
      currentUrl_ = app->addExposedResource(this);
      app_ = app;
      if (c)
        c->addUploadProgressUrl(currentUrl_);
    }
    else
      currentUrl_ = internalPath_;

    return currentUrl_;
  }

//  void WResource::write(WT_BOSTREAM &out,
//                        const Http::ParameterMap &parameters,
//                        const Http::UploadedFileMap &files)
//  {
//    Http::Request request(parameters, files);
//    Http::Response response(this, out);

//    handleRequest(request, response);

//    // While the resource indicates more data to be sent, get it too.
//    while (response.continuation_ && response.continuation_->resource_)
//    {
//      response.continuation_->resource_ = nullptr;
//      request.continuation_ = response.continuation_.get();

//      handleRequest(request, response);
//    }
//  }

  void WResource::setTakesUpdateLock(bool enabled)
  {
    takesUpdateLock_ = enabled;
  }

  unsigned long WResource::version() const
  {
    return version_;
  }

  void WResource::incrementVersion()
  {
    version_++;
  }
}
