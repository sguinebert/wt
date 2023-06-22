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

#ifndef CUEHTTP_RESPONSE_HPP_
#define CUEHTTP_RESPONSE_HPP_

#include <memory>
#include <span>
#include <type_traits>

#include "cookies.hpp"
#include <Wt/fmt/format.h>
#include <Wt/fmt/compile.h>
//#include "detail/body_stream.hpp"
#include "detail/common.hpp"
#include "detail/noncopyable.hpp"
#include "detail/gzip.hpp"
#include "detail/MoveOnlyFunction.hpp"

#include <Wt/WStringStream.h>

/* TODO : replace std::function call by Nano::Signal emit ? */

namespace Wt {
class WResource;

namespace http {

struct Continuation;
class ResponseContinuation;

enum class ResponseType {
    Page,
    Script,
    Update
};

static constexpr std::string_view chunked_end = "0\r\n\r\n";

using namespace std::literals;

class response final : safe_noncopyable {
 public:
    enum class ResponseState {
        ResponseDone,
        ResponseFlush
    };

  response(cookies& cookies, detail::reply_handler handler, detail::reply_handler_sg handler2) noexcept
      : cookies_{cookies}, ostream_(&buffer_),
        last_gmt_date_str_{detail::utils::to_gmt_date_string(std::time(nullptr))},
        reply_handler_{std::move(handler)}, reply_handler_sg_{std::move(handler2)} {}

  void minor_version(unsigned version) noexcept { minor_version_ = version; }

  unsigned status() const noexcept { return status_; }

  void status(unsigned status) { status_ = status; }

  bool has(std::string_view field) const noexcept {
    for (auto it = headers_.begin(); it != headers_.end(); ++it) {
      if (detail::utils::iequals(it->first, field)) {
        return true;
      }
    }
    return false;
  }


  std::string_view get(std::string_view field) const noexcept {
    for (const auto& header : headers_) {
      if (detail::utils::iequals(header.first, field)) {
        return header.second;
      }
    }
    using namespace std::literals;
    return ""sv;
  }

  inline std::string_view getParameter(std::string_view field) const noexcept {
    return get(field);
  }

  template <typename _Field, typename _Value>
  void addHeader(_Field&& field, _Value&& value) {
    headers_.emplace_back(std::make_pair(std::forward<_Field>(field), std::forward<_Value>(value)));
  }

  void set_headers(const std::map<std::string, std::string>& headers) {
    headers_.insert(headers_.end(), headers.begin(), headers.end());
  }

  void set_headers(std::map<std::string, std::string>&& headers) {
    headers_.insert(headers_.end(), std::make_move_iterator(headers.begin()), std::make_move_iterator(headers.end()));
  }

  void remove(std::string_view field) noexcept {
    auto erase = headers_.end();
    for (auto it = headers_.begin(); it != headers_.end(); ++it) {
      if (detail::utils::iequals(it->first, field)) {
        erase = it;
        break;
      }
    }
    if (erase != headers_.end()) {
      headers_.erase(erase);
    }
  }

  template <typename _Url>
  void redirect(_Url&& url) {
    if (status_ == 404) {
      status(302);
    }
    addHeader("Location", std::forward<_Url>(url));
  }

  bool keepalive() const noexcept { return keepalive_; }

  void keepalive(bool keepalive) {
    if (keepalive && minor_version_) {
      keepalive_ = true;
    } else {
      keepalive_ = false;
      addHeader("Connection", "close");
    }
  }

  template <typename _ContentType>
  void type(_ContentType&& content_type) {
    addHeader("Content-Type", std::forward<_ContentType>(content_type));
  }

  void setContentType(std::string_view content_type) {
    addHeader("Content-Type", content_type);
  }

  std::uint64_t length() const noexcept { return content_length_; }

  void length(std::uint64_t content_length) noexcept { content_length_ = content_length; }

  bool has_body() const noexcept { return buffer_.size() != 0; }

  std::string_view dump_body() const noexcept { return std::string_view(boost::asio::buffer_cast<const char*>(buffer_.data()), buffer_.size()); }

  void chunked() noexcept {
    if (!is_chunked_) {
      is_chunked_ = true;
      addHeader("Transfer-Encoding", "chunked");
    }
  }

  template <typename _Body>
  void body(_Body&& body) {
    //if constexpr(std::is_same_v<_Body, const char*>){
    ostream_ << body;
    //}

//    body_ = std::forward<_Body>(body);
//    length(body_.length());
  }

  void body(const char* buffer, std::size_t size) {
    ostream_.write(buffer, size);
    length(size);
  }

  std::ostream& body() {
    return ostream_;
  }
  std::ostream& out() {
    return ostream_;
  }
  /*cancel*/

  /* flush data manually for chunked transfers
  */
  awaitable<void> chunk_flush(bool deflate = false)
  {
    assert(reply_handler_ && is_chunked_);

    auto rawbody = dump_body();
    if (!is_stream_) {
      //is_chunked_ = true;
      is_stream_ = true;
      if(deflate || rawbody.size() > detail::threshold) {
        is_gzip_ = true;
        addHeader("Content-Encoding", "gzip");
      }

      //co_await reply_handler_(header_to_string());

      std::vector<asio::const_buffer> buffers;
      auto cv = header_to_string();
      buffers.push_back(asio::buffer(cv));

      std::string_view corr;
      if(is_gzip_) {

        body_.append(10, '\0');
        detail::gzip::compress(rawbody, body_);
        body_.append("\r\n");

        corr = corrected(body_.data(), body_);
      }
      else {
        ostream_  << "\r\n";

        auto chunk_sv = dump_body();
        auto data = boost::asio::buffer_cast<const char*>(buffer_.data());
        corr = corrected((char*)data, chunk_sv);
      }

      buffers.push_back(asio::buffer(corr));
      co_await reply_handler_sg_(buffers);
      body_.clear();
      co_return;
    }

    if(is_gzip_) {

      body_.append(10, '\0');
      detail::gzip::compress(rawbody, body_);
      body_.append("\r\n");

      auto corr = corrected(body_.data(), body_);
      co_await reply_handler_(corr);
      body_.clear();
    }
    else {
      ostream_  << "\r\n";

      auto chunk_sv = dump_body();
      auto data = boost::asio::buffer_cast<const char*>(buffer_.data());
      auto corr = corrected((char*)data, chunk_sv);

      co_await reply_handler_(corr);
    }

    buffer_.consume(buffer_.size());

    /* reserve space for chunk size + CRLF */
    buffer_.prepare(10);
    buffer_.commit(10);
  }

  void reset() {
    headers_.clear();
    status_ = 404;
    keepalive_ = true;
    content_length_ = 0;
    body_.clear();
    is_chunked_ = false;
    is_stream_ = false;
    is_gzip_ = false;
    response_str_.clear();
    stream_.reset();

    buffer_.consume(buffer_.size());
    buf_.clear();

    haveMoreData_ = nullptr;
    //continuation_.reset();
  }

  bool is_stream() const noexcept { return is_stream_; }

  void to_string(std::string& str) {
    str += detail::utils::get_response_line(minor_version_ * 1000 + status_);
    // headers
    const auto now = std::chrono::steady_clock::now();
    if (now - last_time_ > std::chrono::seconds{1}) {
      last_gmt_date_str_ = detail::utils::to_gmt_date_string(std::time(nullptr));
      last_time_ = now;
    }
    str += last_gmt_date_str_;
    for (const auto& header : headers_) {
      fmt::format_to(std::back_inserter(str), FMT_COMPILE("{}: {}\r\n"), header.first, header.second);
      //str += fmt::format("{}: {}\r\n", header.first, header.second);
    }

    // cookies
    const auto& cookies = cookies_.get();
    for (const auto& cookie : cookies) {
      if (cookie.valid()) {
        fmt::format_to(std::back_inserter(str), FMT_COMPILE("Set-Cookie: {}\r\n"), cookie.to_string());
        //str += fmt::format("Set-Cookie: {}\r\n", cookie.to_string());
      }
    }

    if (!is_chunked_) {
      if (content_length_ != 0) {
        fmt::format_to(std::back_inserter(str), FMT_COMPILE("Content-Length: {}\r\n\r\n"), content_length_);
        //str += fmt::format("Content-Length: {}\r\n\r\n", content_length_);
        str += body_;
      } else {
        str.append("Content-Length: 0\r\n\r\n");
      }
    } else {
      // chunked
      str.append("\r\n");
    }
  }

  std::string_view corrected(char* data, std::string_view sv) {
    std::span<char> s(data, 10);
    fmt::format_to_n(s.begin(), s.size(), FMT_COMPILE("{:>8x}\r\n"), sv.size() - 12);
    auto blank = std::string_view(s.begin(), s.end()).find_first_not_of(' ');
    auto corrected = sv.substr(blank, sv.size() - blank);
    return corrected;
  }

  /* SCATTER GATHER : header buffer (WStringStream) and body buffer (asio::streambuff) */
  void to_buffers(std::vector<asio::const_buffer>& sgbuffers) {

    content_length_ = buffer_.size();


    //auto arr = detail::pooler.malloc();

    if (is_chunked_) { //close the chunked response

      if(content_length_ > 10) {
        ostream_  << "\r\n";
        auto chunk_sv = dump_body();

        if(content_length_ > detail::threshold) {
            auto rawbody = dump_body();
            body_.append(10, '\0');
            detail::gzip::compress(rawbody, body_);
            body_.append("\r\n");

            auto corr = corrected(body_.data(), body_);
            sgbuffers.push_back(asio::buffer(corr));
            addHeader("Content-Encoding", "gzip");
        }
        else {
            auto data = boost::asio::buffer_cast<const char*>(buffer_.data());

            auto corr = corrected((char*)data, chunk_sv);
            //            std::span<char> s((char*)data, 10);
            //            fmt::format_to_n(s.begin(), s.size(), FMT_COMPILE("{:>8x}\r\n"), chunk_sv.size() - 12);
            //            auto blank = std::string_view(s.begin(), s.end()).find_first_not_of(' ');
            //            auto corrected = chunk_sv.substr(blank, chunk_sv.size() - blank);
            sgbuffers.push_back(asio::buffer(corr));
        }
      }

      sgbuffers.push_back(asio::buffer(chunked_end));
      return;
    }

    if(content_length_ > detail::threshold) {
      auto rawbody = dump_body();
      detail::gzip::compress(rawbody, body_);
      content_length_ = body_.size();
    }

    auto cc = detail::utils::get_response_line(minor_version_ * 1000 + status_);
    buf_.append(cc.data(), cc.size());
    // headers
    const auto now = std::chrono::steady_clock::now();
    if (now - last_time_ > std::chrono::seconds{1}) {
      last_gmt_date_str_ = detail::utils::to_gmt_date_string(std::time(nullptr));
      last_time_ = now;
    }

    buf_.append(last_gmt_date_str_.data(), last_gmt_date_str_.size());
    //ostream_ << last_gmt_date_str_;
    for (const auto& header : headers_) {
      //fmt::format_to(std::back_inserter(buf_), FMT_COMPILE("{}: {}\r\n"), header.first, header.second);
      buf_ << fmt::format(FMT_COMPILE("{}: {}\r\n"), header.first, header.second);
    }

    // cookies
    const auto& cookies = cookies_.get();
    for (const auto& cookie : cookies) {
      if (cookie.valid()) {
        //fmt::format_to(std::back_inserter(buf_), FMT_COMPILE("Set-Cookie: {}\r\n"), cookie.to_string());
        buf_ << fmt::format(FMT_COMPILE("Set-Cookie: {}\r\n"), cookie.to_string());
      }
    }

    if (content_length_ != 0) {
      //fmt::format_to(std::back_inserter(buf_), FMT_COMPILE("Content-Length: {}\r\n\r\n"), content_length_);
      buf_ << fmt::format(FMT_COMPILE("Content-Length: {}\r\n\r\n"), content_length_);

      //ostream_ << body_;
    } else {
      buf_ << "Content-Length: 0\r\n\r\n";
    }

    buf_.asioBuffers(sgbuffers);
    if(content_length_)
        body_.empty() ? sgbuffers.push_back(buffer_.data()) : sgbuffers.push_back(asio::buffer(body_));
    //postBuf_.asioBuffers(sgbuffers);
  }
  /* SINGLE BUFFER WRITE : convert header buffer (WStringStream) and body buffer (asio::streambuff) to one dynamic char array (string) */
  void to_strbuffers(std::string& strbuffer) {

    content_length_ = buffer_.size();

    auto cc = detail::utils::get_response_line(minor_version_ * 1000 + status_);
    strbuffer.append(cc.data(), cc.size());
    // headers
    const auto now = std::chrono::steady_clock::now();
    if (now - last_time_ > std::chrono::seconds{1}) {
      last_gmt_date_str_ = detail::utils::to_gmt_date_string(std::time(nullptr));
      last_time_ = now;
    }

    strbuffer.append(last_gmt_date_str_.data(), last_gmt_date_str_.size());
    //ostream_ << last_gmt_date_str_;
    for (const auto& header : headers_) {
      fmt::format_to(std::back_inserter(strbuffer), FMT_COMPILE("{}: {}\r\n"), header.first, header.second);
      //buf_ << fmt::format("{}: {}\r\n", header.first, header.second);
    }

    // cookies
    const auto& cookies = cookies_.get();
    for (const auto& cookie : cookies) {
      if (cookie.valid()) {
        fmt::format_to(std::back_inserter(strbuffer), FMT_COMPILE("Set-Cookie: {}\r\n"), cookie.to_string());
      }
    }

    if (!is_chunked_) {
      if (content_length_ != 0) {
        fmt::format_to(std::back_inserter(strbuffer), FMT_COMPILE("Content-Length: {}\r\n\r\n"), content_length_);

        //ostream_ << body_;
      } else {
        strbuffer += "Content-Length: 0\r\n\r\n";
      }
    } else {
      // chunked
      strbuffer += "\r\n";
    }

    const char* header=boost::asio::buffer_cast<const char*>(buffer_.data());
    strbuffer.append(header, buffer_.size());
  }

  //std::shared_ptr<ResponseContinuation> ResponseContinuationPtr;
  //std::unique_ptr<Continuation> continuation_;
  //WResource* waitingResource_ = nullptr;
  Continuation *continuation_ = nullptr;
  Wt::cpp23::move_only_function<void()> haveMoreData_ = nullptr;
  /* suspend coroutine and restore on the same thread by invoke haveMoreData_ */
  template<class Token>
  auto wait_for_more_data(Token&& handler)
  {
    auto initiator = [this] (auto &&handler) {
        auto ioctx = asio::get_associated_executor(handler);
        haveMoreData_ = [handler = std::move(handler), ioctx] () mutable {
            asio::dispatch(ioctx, [handler = std::move(handler)] () mutable {
                handler();
            });
        };
    };
    return asio::async_initiate<Token, void()>(initiator, handler);
  }

 private:
  std::string header_to_string() {
    std::string str{detail::utils::get_response_line(minor_version_ * 1000 + status_)};
    // headers
    // os << detail::utils::to_gmt_date_string(std::time(nullptr));

    const auto now = std::chrono::steady_clock::now();
    if (now - last_time_ > std::chrono::seconds{1}) {
      //fmt::format_to(std::back_inserter(str), "{:%a, %d %b %Y %T} GMT\r\n", now);
      //last_gmt_date_str_ = fmt::format(FMT_COMPILE("{:%a, %d %b %Y %T} GMT\r\n"),  std::chrono::system_clock::now());
      last_gmt_date_str_ = detail::utils::to_gmt_date_string(std::time(nullptr));
      last_time_ = now;
    }
    str += last_gmt_date_str_;

    for (const auto& header : headers_) {
      fmt::format_to(std::back_inserter(str), FMT_COMPILE("{}: {}\r\n"), header.first, header.second);
      //str += fmt::format("{}: {}\r\n", header.first, header.second);
    }

    if (get("connection").empty() && keepalive_) {
      str += "Connection: keep-alive\r\n"sv;
    }

    // cookies
    const auto& cookies = cookies_.get();
    for (const auto& cookie : cookies) {
      if (cookie.valid()) {
        fmt::format_to(std::back_inserter(str), FMT_COMPILE("Set-Cookie: {}\r\n"), cookie.to_string());
        //str += fmt::format("Set-Cookie: {}\r\n", cookie.to_string());
      }
    }

    if (is_chunked_) {
      str += "\r\n"sv;
    } else {
      fmt::format_to(std::back_inserter(str), FMT_COMPILE("Content-Length: {}\r\n\r\n"), content_length_);
      //str += fmt::format("Content-Length: {}\r\n\r\n", content_length_);
    }

    return str;
  }

  void setResponseType(ResponseType responseType) { responseType_ = responseType; }
  ResponseType responseType() const { return responseType_; }

private:
  std::vector<std::pair<std::string, std::string>> headers_;
  unsigned minor_version_{1};
  unsigned status_{404};
  bool keepalive_{true};
  std::uint64_t content_length_{0};
  cookies& cookies_;

  ResponseType responseType_;
  std::string body_;
  std::string response_str_;

  Wt::WStringStream buf_;
  Wt::WStringStream postBuf_;
  asio::streambuf buffer_;
  std::ostream ostream_;

  bool is_chunked_{false};
  bool is_stream_{false};
  bool is_gzip_ {false};
  std::chrono::steady_clock::time_point last_time_{std::chrono::steady_clock::now()};
  std::string last_gmt_date_str_;
  detail::reply_handler reply_handler_;
  detail::reply_handler_sg reply_handler_sg_;
  std::shared_ptr<std::ostream> stream_{nullptr};

  friend class context;
};

struct Continuation {
  Continuation(Wt::WResource* resource, http::response *response) : resource_(resource), response_(response)
  { }
  ~Continuation() {
    //resource_->removeContinuation(this);
    resource_ = nullptr;
    response_ = nullptr;
  }

  void haveMoreData() {
    if(response_ && response_->haveMoreData_)
      response_->haveMoreData_();
  }

  void destroy() {
    response_ = nullptr;
  }

  /* Call cancel function in connection */
  void cancel()
  {
    if(response_){
      std::cout << "cancel the response " << std::endl;
    }
  }

  bool destroyed() const { return response_ == nullptr; }

  //bool use(WResource *resource);

  private:
  //bool ready_ = true;
  Wt::WResource *resource_;
  http::response *response_;
  //std::atomic<bool> invalid_ = false; //std::mutex resource_mutex_;
};

}  // namespace http
}  // namespace cue

#endif  // CUEHTTP_RESPONSE_HPP_
