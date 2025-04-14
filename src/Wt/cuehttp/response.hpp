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

static inline constexpr std::string_view chunked_head = "\0\0\0\0\0\0\0\0\0\0";
static inline constexpr std::string_view chunked_end = "0\r\n\r\n";
static inline constexpr std::string_view CRLF = "\r\n";

static thread_local std::chrono::steady_clock::time_point last_time_{std::chrono::steady_clock::now()};
static thread_local std::string last_gmt_date_str_ {64, '\0'};

using namespace std::literals;

class response final : safe_noncopyable {
 public:
    enum class ResponseState {
        ResponseDone,
        ResponseFlush
    };

  response(cookies& cookies, detail::reply_handler handler, detail::reply_handler_sg handler2) noexcept
      : cookies_{cookies}, ostream_(&buffer_),
        //last_gmt_date_str_{detail::utils::to_gmt_date_string(std::time(nullptr))},
        reply_handler_{std::move(handler)}, reply_handler_sg_{std::move(handler2)} {}

  response(cookies& cookies, asio::streambuf& ostream) noexcept
      : cookies_{cookies}, ostream_(&ostream),
      //last_gmt_date_str_{detail::utils::to_gmt_date_string(std::time(nullptr))},
      reply_handler_{}, reply_handler_sg_{} {}

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

  //std::string_view dump_body() const noexcept { return std::string_view(boost::asio::buffer_cast<const char*>(buffer_.data()), buffer_.size()); }
  std::string_view dump_body() const noexcept { return std::string_view(body_buffer_.data(), body_buffer_.size()); }
  std::string_view prefixed_body() const noexcept { return std::string_view(body_buffer_.data(), body_buffer_.size()); }
  std::string_view body() const noexcept { return std::string_view(body_buffer_.begin() + chunked_head.size(), body_buffer_.end()); }

  void chunked() noexcept {
    if (!is_chunked_) {
      is_chunked_ = true;
      addHeader("Transfer-Encoding", "chunked");
    }
  }

  template <typename _Body>
  void body(_Body&& body) {
      body_buffer_.append(body.data(), body.data() + body.length());
    //if constexpr(std::is_same_v<_Body, const char*>){
    //ostream_ << body;
    //}

//    body_ = std::forward<_Body>(body);
//    length(body_.length());
  }

  auto operator<<(std::string_view body) -> response& {
    body_buffer_.append(body.data(), body.data() + body.length());
    return *this;
  }

  void body(const char* buffer, std::size_t size) {
    body_buffer_.append(buffer, buffer + size);

    // ostream_.write(buffer, size);
    length(size);
  }

  std::ostream& bodystd() {
    return ostream_;
  }
  std::ostream& outstd() {
    return ostream_;
  }

  auto out() -> std::back_insert_iterator<fmt::memory_buffer> {
    return std::back_inserter(body_buffer_);
  }

  auto buffer() -> fmt::memory_buffer& {
    return body_buffer_;
  }

  void write(char* data, std::size_t size) {
    body_buffer_.append(data, data + size);
  }
  /*cancel*/

  /* flush data manually for chunked transfers
   * why so many effort to use prepend body_buffer_ or deflated_body_ with size of the chunk?
   * because of performance gain for contiguous memory vs scatter gather (cf asio performance benchmark)
  */
  awaitable<void> chunk_flush(bool deflate = false)
  {
    content_length_ = 0;
    assert(reply_handler_ && is_chunked_);

    auto rawbody = body();
    if (!is_stream_) { //if not already streaming,
      //is_chunked_ = true;
      is_stream_ = true;
      if(deflate || rawbody.size() > detail::threshold) {
        deflate_gzip_ = true;
        addHeader("Content-Encoding", "gzip");
      }

      //co_await reply_handler_(header_to_string());

      std::vector<asio::const_buffer> buffers;
      auto headers = header_to_string();
      buffers.push_back(asio::buffer(headers));

      std::string_view corr;
      if(deflate_gzip_) {

        deflated_body_.append(10, '\0'); //prepend 10 x '\0' to prepare space for chunk size + CRLF
        detail::gzip::compress(rawbody, deflated_body_);
        deflated_body_.append("\r\n"); //CRLF append

        corr = corrected(deflated_body_); //add length of chunk to the prepended 10 x '\0'
      }
      else {
        //ostream_  << "\r\n";
        body_buffer_.append(CRLF); //CRLF append

        auto chunk_sv = prefixed_body();
        corr = corrected(chunk_sv);
      }

      buffers.push_back(asio::buffer(corr));
      co_await reply_handler_sg_(buffers);
      deflated_body_.clear();
      co_return;
    }

    //after the first chunk, we are streaming
    if(deflate_gzip_ && !is_deflated_) { //check if already done for static data (ie WResource)

      deflated_body_.append(10, '\0');
      detail::gzip::compress(rawbody, deflated_body_);
      deflated_body_.append("\r\n"); //CRLF append

      auto corr = corrected(deflated_body_);
      co_await reply_handler_(corr); //single contiguous memory (no scatter gather sys call)
      deflated_body_.clear();
    }
    else {
      //ostream_  << "\r\n";
      body_buffer_.append(CRLF); //CRLF append

      auto chunk_sv = prefixed_body();
      auto corr = corrected(chunk_sv);

      co_await reply_handler_(corr); //single contiguous memory (no scatter gather sys call)
    }

    // buffer_.consume(buffer_.size());
    /* reserve space for chunk size + CRLF */
    // buffer_.prepare(10);
    // buffer_.commit(10);


    /* reserve space for chunk size + CRLF */
    body_buffer_.clear();
    body_buffer_.append(chunked_head); //prepend 10 x '\0' to prepare space for chunk size + CRLF
  }

  void reset() {
    headers_.clear();
    status_ = 404;
    keepalive_ = true;
    content_length_ = 0;
    deflated_body_.clear();
    is_chunked_ = false;
    is_stream_ = false;
    deflate_gzip_ = false;

    response_str_.clear();//deprecated
    stream_.reset();//deprecated
    buffer_.consume(buffer_.size()); //Deprecated
    buf_.clear();//deprecated

    haveMoreData_ = nullptr;

    headers_buffer_.clear();
    /* reserve space for chunk size + CRLF */
    body_buffer_.clear();
    body_buffer_.append(chunked_head); //prepend 10 x '\0' to prepare space for chunk size + CRLF
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
        str += deflated_body_;
      } else {
        str.append("Content-Length: 0\r\n\r\n");
      }
    } else {
      // chunked
      str.append("\r\n");
    }
  }

  //add length of chunk to the prepended 10 x '\0' - modify the first 10 bytes of the chunk (std::string_view)
  std::string_view corrected(/*char* data,*/ std::string_view sv) {
    std::span<char> s((char*)sv.data(), 10);
    //write the length of the chunk to the right of the 10 x '\0' and fill left with ' ' char
    fmt::format_to_n(s.begin(), s.size(), FMT_COMPILE("{:>8x}\r\n"), sv.size() - 12);
    //dump the fill ' ' char from the chunk view
    auto blank = std::string_view(s.begin(), s.end()).find_first_not_of(' ');
    auto corrected = sv.substr(blank, sv.size() - blank);
    return corrected;
  }

  /* SCATTER GATHER : header buffer (WStringStream) and body buffer (asio::streambuff) */
  void to_buffers(std::vector<asio::const_buffer>& sgbuffers) {

    content_length_ = body().size();
    //auto arr = detail::pooler.malloc();

    if (is_chunked_) { //close the chunked response
      if(content_length_) { //if for any reason after chunk calls the body_buffer_ is not empty, we need to send the last chunk
        //ostream_  << "\r\n";
        body_buffer_.append(CRLF);
        auto chunk_sv = prefixed_body();

        if(content_length_ > detail::threshold) {
            auto rawbody = body();
            deflated_body_.append(10, '\0');
            detail::gzip::compress(rawbody, deflated_body_);
            deflated_body_.append("\r\n");

            auto corr = corrected(deflated_body_);
            sgbuffers.push_back(asio::buffer(corr));
            addHeader("Content-Encoding", "gzip");
        }
        else {
            auto corr = corrected(chunk_sv);
            sgbuffers.push_back(asio::buffer(corr));
        }
      }

      sgbuffers.push_back(asio::buffer(chunked_end));
      return;
    }

    if(content_length_ > detail::threshold) {
      auto rawbody = body();
      detail::gzip::compress(rawbody, deflated_body_);
      content_length_ = deflated_body_.size();
    }

    //buf_.append(cc.data(), cc.size());
    headers_buffer_.append(detail::utils::get_response_line(minor_version_ * 1000 + status_));
    // headers
    const auto now = std::chrono::steady_clock::now();
    if (now >= last_time_) {
        const auto sys_now = std::chrono::system_clock::now();
        auto ret = fmt::format_to_n(last_gmt_date_str_.begin(), 64, FMT_COMPILE("{:%a, %d %b %Y %T} GMT\r\n"), fmt::gmtime(sys_now));
        last_gmt_date_str_.erase(ret.size);
        //last_gmt_date_str_ = detail::utils::to_gmt_date_string(std::time(nullptr));
        last_time_ = now + std::chrono::seconds{1};
    }

    //buf_.append(last_gmt_date_str_.data(), last_gmt_date_str_.size());
    //ostream_ << last_gmt_date_str_;
    headers_buffer_.append(last_gmt_date_str_);
    for (const auto& header : headers_) {
      fmt::format_to(std::back_inserter(headers_buffer_), FMT_COMPILE("{}: {}\r\n"), header.first, header.second);
      //buf_ << fmt::format(FMT_COMPILE("{}: {}\r\n"), header.first, header.second);
    }

    // cookies
    // const auto& cookies = cookies_.get();
    // for (const auto& cookie : cookies) {
    //   if (cookie.valid()) {
    //     //fmt::format_to(std::back_inserter(buf_), FMT_COMPILE("Set-Cookie: {}\r\n"), cookie.to_string());
    //     buf_ << fmt::format(FMT_COMPILE("Set-Cookie: {}\r\n"), cookie.to_string());
    //   }
    // }
    fmt::format_to(std::back_inserter(headers_buffer_), FMT_COMPILE("{}"), cookies_.get());


    fmt::format_to(std::back_inserter(headers_buffer_), FMT_COMPILE("Content-Length: {}\r\n\r\n"), content_length_);


    sgbuffers.emplace_back(asio::buffer(headers_buffer_.data(), headers_buffer_.size()));

    //buf_.asioBuffers(sgbuffers);
    if(content_length_)
        deflated_body_.empty() ? sgbuffers.emplace_back(asio::buffer(body_buffer_.data(), body_buffer_.size())) : sgbuffers.emplace_back(asio::buffer(deflated_body_));
    //postBuf_.asioBuffers(sgbuffers);
  }

  /* SINGLE BUFFER WRITE : NOT USED (EXPERIMENTAL) convert header buffer (WStringStream) and body buffer (asio::streambuff) to one dynamic char array (string) */
  void to_buffers(fmt::memory_buffer& fmtbuffer) {

    content_length_ = body().size();

    if (is_chunked_) { //close the chunked response
      if(content_length_) { //if for any reason after chunk calls the body_buffer_ is not empty, we need to send the last chunk
          body_buffer_.append(CRLF);
          auto chunk_sv = prefixed_body();

          if(content_length_ > detail::threshold) {
              auto rawbody = body();
              deflated_body_.append(10, '\0');
              detail::gzip::compress(rawbody, deflated_body_);
              deflated_body_.append("\r\n");

              auto corr = corrected(deflated_body_);
              fmtbuffer.append(corr);
              addHeader("Content-Encoding", "gzip");
          }
          else {
              auto corr = corrected(chunk_sv);
              fmtbuffer.append(corr);
          }
      }
      fmtbuffer.append(chunked_end);
      return;
    }

    // headers
    fmtbuffer.append(detail::utils::get_response_line(minor_version_ * 1000 + status_));
    const auto now = std::chrono::steady_clock::now();
    if (now >= last_time_) {
        const auto sys_now = std::chrono::system_clock::now();
        auto ret = fmt::format_to_n(last_gmt_date_str_.begin(), 64, FMT_COMPILE("{:%a, %d %b %Y %T} GMT\r\n"), fmt::gmtime(sys_now));
        last_gmt_date_str_.erase(ret.size);
        last_time_ = now + std::chrono::seconds{1};
    }

    fmtbuffer.append(last_gmt_date_str_);
    //ostream_ << last_gmt_date_str_;
    for (const auto& header : headers_) {
      fmt::format_to(std::back_inserter(fmtbuffer), FMT_COMPILE("{}: {}\r\n"), header.first, header.second);
      //buf_ << fmt::format("{}: {}\r\n", header.first, header.second);
    }

    // cookies
    fmt::format_to(std::back_inserter(fmtbuffer), FMT_COMPILE("{}"), cookies_.get());

    if (!is_chunked_) {
        fmt::format_to(std::back_inserter(fmtbuffer), FMT_COMPILE("Content-Length: {}\r\n\r\n"), content_length_);
    } else {
      // chunked
      fmtbuffer.append(CRLF);
    }

    if(content_length_ > detail::threshold) {
        auto rawbody = body();
        auto presize = fmtbuffer.size();
        detail::gzip::compress(rawbody, fmtbuffer);
        content_length_ = fmtbuffer.size() - presize;
    }
    else {
        fmtbuffer.append(body());
    }
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
  std::string_view header_to_string() {
      headers_buffer_.clear();
      headers_buffer_.append(detail::utils::get_response_line(minor_version_ * 1000 + status_));

      const auto now = std::chrono::steady_clock::now();
      if (now >= last_time_) {
          const auto sys_now = std::chrono::system_clock::now();
          auto ret = fmt::format_to_n(last_gmt_date_str_.begin(), 64, FMT_COMPILE("{:%a, %d %b %Y %T} GMT\r\n"), fmt::gmtime(sys_now));
          last_gmt_date_str_.erase(ret.size);
          last_time_ = now + std::chrono::seconds{1};
      }
      headers_buffer_.append(last_gmt_date_str_);
      for (const auto& header : headers_) {
          fmt::format_to(std::back_inserter(headers_buffer_), FMT_COMPILE("{}: {}\r\n"), header.first, header.second);
      }

      if (get("connection").empty() && keepalive_) {
          headers_buffer_.append("Connection: keep-alive\r\n"sv);
      }
      fmt::format_to(std::back_inserter(headers_buffer_), FMT_COMPILE("{}"), cookies_.get());
      if (is_chunked_) { //"{:x}\r\n"
          headers_buffer_.append("\r\n"sv);
      } else {
          fmt::format_to(std::back_inserter(headers_buffer_), FMT_COMPILE("Content-Length: {}\r\n\r\n"), content_length_);
      }

    // std::string str{detail::utils::get_response_line(minor_version_ * 1000 + status_)};

    // const auto now = std::chrono::steady_clock::now();
    // if (now >= last_time_) {
    //   const auto sys_now = std::chrono::system_clock::now();
    //   auto ret = fmt::format_to_n(last_gmt_date_str_.begin(), 64, FMT_COMPILE("{:%a, %d %b %Y %T} GMT\r\n"), fmt::gmtime(sys_now));
    //   last_gmt_date_str_.erase(ret.size);
    //   last_time_ = now + std::chrono::seconds{1};
    // }
    // str += last_gmt_date_str_;

    // for (const auto& header : headers_) {
    //   fmt::format_to(std::back_inserter(str), FMT_COMPILE("{}: {}\r\n"), header.first, header.second);
    // }

    // if (get("connection").empty() && keepalive_) {
    //   str += "Connection: keep-alive\r\n"sv;
    // }

    // cookies
    //const auto& cookies = cookies_.get();
    // for (const auto& cookie : cookies) {
    //   if (cookie.valid()) {
    //     fmt::format_to(std::back_inserter(str), FMT_COMPILE("Set-Cookie: {}\r\n"), cookie.to_string());
    //     //str += fmt::format("Set-Cookie: {}\r\n", cookie.to_string());
    //   }
    // }
    //fmt::format_to(std::back_inserter(str), FMT_COMPILE("{}"), cookies_.get());

    // if (is_chunked_) { //"{:x}\r\n"
    //   str += "\r\n"sv;
    // } else {
    //   fmt::format_to(std::back_inserter(str), FMT_COMPILE("Content-Length: {}\r\n\r\n"), content_length_);
    // }

    return std::string_view(headers_buffer_.begin(), headers_buffer_.end());
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
  std::string response_str_; //Deprecated
  Wt::WStringStream buf_; //Deprecated
  Wt::WStringStream postBuf_; //Deprecated
  asio::streambuf buffer_; //Deprecated
  std::ostream ostream_; //Deprecated

  /* buffer & view for body response */
  fmt::memory_buffer headers_buffer_; //
  fmt::memory_buffer body_buffer_; //experimental
  std::string deflated_body_; //gzip or equivalent deflated body (body_buffer_ deflated)
  std::string_view body_buffer_sv_; //alternative to body_buffer_ to avoid copy for WResource objects

  bool is_chunked_{false};
  bool is_stream_{false};
  bool deflate_gzip_ {false}, is_deflated_{false};
  // static thread_local std::chrono::steady_clock::time_point last_time_{std::chrono::steady_clock::now()};
  // static thread_local std::string last_gmt_date_str_ {64, '\0'};
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

    response_->haveMoreData_ = nullptr;
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
