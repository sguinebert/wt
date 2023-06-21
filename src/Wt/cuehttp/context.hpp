/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef CUEHTTP_CONTEXT_HPP_
#define CUEHTTP_CONTEXT_HPP_

#include <memory>

#include "cookies.hpp"
#include "detail/noncopyable.hpp"
#include "request.hpp"
#include "response.hpp"
#include "session.hpp"
#include "websocket.hpp"
#include "detail/MoveOnlyFunction.hpp"

#include <Wt/Http/Request.h>


namespace Wt {
namespace http {

class context final : safe_noncopyable {
    friend class CgiParser;
 public:
    context(detail::reply_handler handler,  detail::reply_handler_sg handler2, bool https, detail::ws_send_handler ws_send_handler) noexcept
         : response_{cookies_, std::move(handler), std::move(handler2)},
        request_{https, response_, cookies_},
        ws_send_handler_{std::move(ws_send_handler)} {}

  request& req() noexcept { return request_; }

  response& res() noexcept { return response_; }

  class websocket& websocket() {
    make_ws();
    return *websocket_;
  }

  bool isWebSocketMessage() { return websocket_ != nullptr; }

  std::shared_ptr<class websocket> websocket_ptr() {
    make_ws();
    return websocket_;
  }

  // request
  std::vector<std::pair<std::string_view, std::string_view>> headers() const noexcept { return request_.headers(); }

  std::string_view getHeader(std::string_view field) const noexcept { return request_.get(field); }

  std::string_view getParameter(const std::string& field) const noexcept { return request_.getParameter(field); }

  std::string_view method() const noexcept { return request_.method(); }

  std::string_view host() const noexcept { return request_.host(); }

  std::string_view port() const noexcept { return request_.port(); }

  std::string_view hostname() const noexcept { return request_.hostname(); }

  std::string_view url() const noexcept { return request_.url(); }

  boost::url_view& urlv() noexcept { return request_.urlv(); }

  std::string_view origin() const noexcept { return request_.origin(); }

  std::string_view href() const noexcept { return request_.href(); }

  std::string_view path() const noexcept { return request_.path(); }

  std::string_view pathInfo(std::string_view base) noexcept { return request_.pathInfo(base); }
  std::string_view pathInfo() const noexcept { return request_.pathInfo(); }

  std::string_view querystring() const noexcept { return request_.querystring(); }

  UploadedFileMap& uploadedFiles() { return request_.files(); }

  //const UploadedFileMap& uploadedFiles() const { return request_.files(); }

//#ifdef HTTP_WITH_SSL
//  void registerSslHandle(SSL *ssl) { request_.ssl = ssl; }
//#endif
#ifdef WT_WITH_SSL
  SSL_CTX* ssl_ = nullptr;
  void setSSLcontext(SSL_CTX* ssl_ctx) { ssl_ = ssl_ctx; }
#endif

  // response
  unsigned status() const noexcept { return response_.status(); }

  void status(unsigned status) noexcept { response_.status(status); }

  void setResponseType(ResponseType responseType) { response_.setResponseType(responseType); }
  ResponseType responseType() const { return response_.responseType(); }

  template <typename _Url>
  void redirect(_Url&& url) {
    response_.redirect(std::forward<_Url>(url));
  }

  template <typename _Field, typename _Value>
  void addHeader(_Field&& field, _Value&& value) {
    response_.addHeader(std::forward<_Field>(field), std::forward<_Value>(value));
  }

  template <typename _Headers>
  void set_headers(_Headers&& headers) {
    response_.set_headers(std::forward<_Headers>(headers));
  }

  void remove(std::string_view field) noexcept { response_.remove(field); }

  template <typename _ContentType>
  void type(_ContentType&& content_type) {
    response_.type(std::forward<_ContentType>(content_type));
  }

  void setContentType(std::string_view content_type) {
    type(content_type);
  }

  void length(std::uint64_t content_length) noexcept { response_.length(content_length); }

  auto prepare(std::size_t n) { return response_.buffer_.prepare(n); }
  void commit(std::size_t n) { response_.buffer_.commit(n); }

  class cookies& cookies() noexcept {
    return cookies_;
  }

  bool session_enabled() const noexcept { return !!session_; }

  class session& session() {
    assert(session_);
    return *session_;
  }

  template <typename _Options>
  void session(_Options&& options) {
    session_ = std::make_unique<class session>(std::forward<_Options>(options), *this, cookies_);
  }

  void chunked() noexcept { response_.chunked(); }

  bool has_body() const noexcept { return response_.has_body(); }

  bool has_header(std::string_view field) const noexcept { return request_.has_header(field); }

  bool has_parameter(const std::string& field) const noexcept { return request_.query().contains(field); }

  template <typename _Body>
  void body(_Body&& body) {
    response_.body(std::forward<_Body>(body));
  }

  void body(const char* buffer, std::size_t size) { response_.body(buffer, size); }

  std::ostream& body() { return response_.body(); }

  std::ostream& out() { return response_.body(); }

  void reset() {
    flush_ = false;
    request_.reset();
    response_.reset();
    cookies_.reset();
  }

  template<class Token>
  auto wait_flush(Token&& handler)
  {
    auto initiator = [this] (auto &&handler) {
        writecallback_ = [handler = std::move(handler)] () mutable {
            handler();
        };
    };
    return asio::async_initiate<Token, void()>(initiator, handler);
  }

  void flush() {
    flush_ = true;

    if(websocket_) {
        if(auto sv = response_.dump_body(); !sv.empty())
            websocket_->send(std::string(sv));
        response_.reset();
        request_.reset();
        return;
    }

    if(writecallback_)
        writecallback_();

    writecallback_ = nullptr;
  }

  std::shared_ptr<Wt::WebSession> websession() { return websession_; }
  void websession(std::shared_ptr<Wt::WebSession> wsession) { websession_ = wsession; }

  bool flush_ = false;
  ::int64_t postDataExceeded() noexcept { return request_.postDataExceeded_; }

 private:
  void make_ws() {
    //assert(request_.websocket());
    if (!websocket_) {
      websocket_ = std::make_shared<class websocket>(ws_send_handler_);
    }
  }

  class cookies cookies_;
  response response_;
  request request_;
  std::shared_ptr<class websocket> websocket_{nullptr};
  detail::ws_send_handler ws_send_handler_;
  std::unique_ptr<class session> session_{nullptr};
  std::shared_ptr<Wt::WebSession> websession_;

  Wt::cpp23::move_only_function<void()> writecallback_ = nullptr;
};

}  // namespace http
}  // namespace cue

#endif  // CUEHTTP_CONTEXT_HPP_
