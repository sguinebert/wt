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

#ifndef CUEHTTP_REQUEST_HPP_
#define CUEHTTP_REQUEST_HPP_

#include <algorithm>
#include <memory>
#include <vector>

#include "cookies.hpp"
#include "deps/picohttpparser.h"
#include "detail/common.hpp"
#include "detail/noncopyable.hpp"
#include "response.hpp"

#include <Wt/Http/Request.h>
#include <boost/url.hpp>
#include <boost/spirit/home/x3.hpp>
//#include <Wt/Configuration.h>

namespace x3 = boost::spirit::x3;

namespace Wt {
namespace http {

/*! \brief A list of parameter values.
 *
 * This is the type used to aggregate all values for a single parameter.
 */
#ifndef WT_TARGET_JAVA
typedef std::vector<std::string> ParameterValues;
#else
typedef std::string ParameterValues[];
#endif

typedef std::unordered_map<std::string, ParameterValues> ParameterMap;

/*! \brief A file parameter map.
 *
 * This is the type used aggregate file parameter values in a request.
 */
typedef std::unordered_multimap<std::string, Http::UploadedFile> UploadedFileMap;

class request final : safe_noncopyable {
public:
    /*! \brief A single byte range.
   */
    class WT_API ByteRange
    {
    public:
        /*! \brief Creates a (0,0) byteranges */
        ByteRange() : firstByte_(0), lastByte_(0)
        { }

        /*! \brief Creates a byte range.
     */
        ByteRange(::uint64_t first, ::uint64_t last)
            : firstByte_(first),
            lastByte_(last)
        {
        }

        /*! \brief Returns the first byte of this range.
     */
        ::uint64_t firstByte() const { return firstByte_; }

        /*! \brief Returns the last byte of this range.
     */
        ::uint64_t lastByte() const { return lastByte_; }

    private:
        ::uint64_t firstByte_, lastByte_;
    };

    /*! \brief A byte range specifier.
   *
   * \sa getRanges()
   */
    class WT_API ByteRangeSpecifier : public std::vector<ByteRange>
    {
    public:
        /*! \brief Creates an empty byte range specifier.
     *
     * The specifier is satisfiable but empty, indicating that no
     * ranges were present.
     */
        ByteRangeSpecifier() : satisfiable_(true)
        {
        }

        /*! \brief Returns whether the range is satisfiable.
     *
     * If the range specification is not satisfiable, RFC 2616 states you
     * should return a response status of 416. isSatisfiable() will return
     * true if a Range header was missing or a syntax error occured, in
     * which case the number of ByteRanges will be zero and the client
     * must send the entire file.
     */
        bool isSatisfiable() const { return satisfiable_; }

        /*! \brief Sets whether the specifier is satisfiable.
     */
        void setSatisfiable(bool satisfiable) { satisfiable_ = satisfiable; }

    private:
        bool satisfiable_;
    };

    void inplaceUrlDecode(std::string &text)
    {
        // Note: there is a Java-too duplicate of this function in Wt/Utils.C
        std::size_t j = 0;

        for (std::size_t i = 0; i < text.length(); ++i)
        {
            char c = text[i];

            if (c == '+')
            {
                text[j++] = ' ';
            }
            else if (c == '%' && i + 2 < text.length())
            {
                std::string h = text.substr(i + 1, 2);
                char *e = 0;
                int hval = std::strtol(h.c_str(), &e, 16);

                if (*e == 0)
                {
                    text[j++] = (char)hval;
                    i += 2;
                }
                else
                {
                    // not a proper %XX with XX hexadecimal format
                    text[j++] = c;
                }
            }
            else
                text[j++] = c;
        }

        text.erase(j);
    }

    void parseFormUrlEncoded(std::string_view s, ParameterMap &parameters)
    {
//        std::vector<std::string_view> parse;
//        parseStringView(s, parse);
        for (std::size_t pos = 0; pos < s.length();)
        {
            std::size_t next = s.find_first_of("&=", pos);

            if (next == pos && s[next] == '&')
            {
                // skip empty
                pos = next + 1;
                continue;
            }

            if (next == std::string::npos || s[next] == '&')
            {
                if (next == std::string::npos)
                    next = s.length();
                std::string key { s.substr(pos, next - pos) };
                inplaceUrlDecode(key);
                parameters[key].push_back(std::string());
                pos = next + 1;
            }
            else
            {
                std::size_t amp = s.find('&', next + 1);
                if (amp == std::string::npos)
                    amp = s.length();

                std::string key { s.substr(pos, next - pos) };
                inplaceUrlDecode(key);

                std::string value { s.substr(next + 1, amp - (next + 1)) };
                inplaceUrlDecode(value);

                parameters[key].push_back(value);
                pos = amp + 1;
            }
        }
    }

    void parseFormUrlEncoded(std::string_view s)
    {
        for (std::size_t pos = 0; pos < s.length();)
        {
            std::size_t next = s.find_first_of("&=", pos);

            if (next == pos && s[next] == '&')
            {
                // skip empty
                pos = next + 1;
                continue;
            }

            if (next == std::string::npos || s[next] == '&')
            {
                if (next == std::string::npos)
                    next = s.length();
                std::string key { s.substr(pos, next - pos) };
                inplaceUrlDecode(key);
                query_.emplace(std::move(key), std::string());
                pos = next + 1;
            }
            else
            {
                std::size_t amp = s.find('&', next + 1);
                if (amp == std::string::npos)
                    amp = s.length();

                std::string key { s.substr(pos, next - pos) };
                inplaceUrlDecode(key);

                std::string value { s.substr(next + 1, amp - (next + 1)) };
                inplaceUrlDecode(value);

                query_.emplace(std::move(key), std::move(value));
                pos = amp + 1;
            }
        }
    }

    void parseFormUrlEncoded(std::vector<std::pair<std::string_view, std::string_view>>& s)
    {
        for(auto &[keysv, valsv] : s) {
            std::string key { keysv };
            inplaceUrlDecode(key);

            std::string val { valsv };
            inplaceUrlDecode(val);
            query_.emplace(std::move(key), std::move(val));
        }
    }

 public:
  request(bool https, response& res, cookies& cookies) noexcept
      : https_{https}, buffer_(HTTP_REQUEST_BUFFER_SIZE), res_{res}, cookies_{cookies} {}

  unsigned minor_version() const noexcept { return minor_version_; }

  std::string_view get(std::string_view field) const noexcept {
    for (std::size_t i{0}; i < phr_num_headers_; ++i) {
      const auto& header = phr_headers_[i];
      if (detail::utils::iequals({header.name, header.name_len}, field)) {
        return {header.value, header.value_len};
      }
    }

    using namespace std::literals;
    return ""sv;
  }

  bool has_header(std::string_view field) const noexcept {
    for (std::size_t i{0}; i < phr_num_headers_; ++i) {
      const auto& header = phr_headers_[i];
      if (detail::utils::iequals({header.name, header.name_len}, field)) {
        return true;
      }
    }
    return false;
  }

  std::vector<std::pair<std::string_view, std::string_view>> headers() const noexcept {
    std::vector<std::pair<std::string_view, std::string_view>> headers;
    for (std::size_t i{0}; i < phr_num_headers_; ++i) {
      const auto& header = phr_headers_[i];
      headers.emplace_back(std::string_view{header.name, header.name_len},
                           std::string_view{header.value, header.value_len});
    }
    return headers;
  }

  std::string_view method() const noexcept { return method_; }

  std::string_view host() const noexcept { return get("Host"); }

  std::string_view hostname() const noexcept {
    const auto host_str = host();
    return host_str.substr(0, host_str.rfind(":"));
  }
  std::string_view port() const noexcept {
    const auto host_str = host();
    return host_str.substr(host_str.rfind(":"));
  }

  std::string_view url() const noexcept { return url_; }

  boost::url_view& urlv() noexcept { return urlv_; }

  std::string_view origin() const noexcept {
    if (origin_.empty()) {
      origin_ = https_ ? "https://" : "http://";
      origin_ += host();
    }
    return origin_;
  }

  std::string_view scheme() const noexcept {
    return https_ ? "https" : "http";;
  }

  std::string_view href() const noexcept {
    if (href_.empty()) {
      href_ += origin();
      href_ += url_;
    }
    return href_;
  }

  std::string_view path() const noexcept { return path_; }

  std::string_view pathInfo(std::string_view base) noexcept { if(pathInfo_.empty() && path_.size() != base.size()) pathInfo_ = path_.substr(base.size() + 1); return pathInfo_; }
  std::string_view pathInfo() const noexcept { return pathInfo_; }

  std::string_view querystring() const noexcept { return querystring_; }

  UploadedFileMap& files() { return files_; }
  UploadedFileMap& uploadedFiles() { return files_; }

  std::multimap<std::string, std::string>& query() const noexcept {
    if (!querystring_.empty() && query_.empty()) {
      query_ = detail::utils::parse_query(querystring_);
    }
    return query_;
  }

  std::string_view getParameter(const std::string& key) const {
    const auto& mm = query();
    if (auto it = mm.find(key); it != mm.end()) {
      return it->second;
    }
    return ""sv;
  }

  mutable  std::vector<std::string> aVector; //risk of dangling reference in FormatData
  std::vector<std::string>& getParameterValues(const std::string& key) const {
    const auto& mm = query();
    auto aRange = mm.equal_range(key);
    aVector.clear();
    std::transform(aRange.first, aRange.second,std::back_inserter(aVector), [](auto element){ return element.second;});
    return aVector;
  }

  ParameterMap& getParameters() {
    const auto& mm = query();
    if (!querystring_.empty() && parameters_.empty()) {
      for (auto it = mm.begin(); it != mm.end(); ++it) {
        auto aRange = mm.equal_range(it->first);
        std::vector<std::string> aVector;
        std::transform(aRange.first, aRange.second,std::back_inserter(aVector), [](auto element){ return std::string(element.second);});
        parameters_[it->first] = aVector;
      }
    }
    return parameters_;
  }

  std::string_view search() const noexcept { return search_; }

  std::string_view type() const noexcept {
    const auto content_type = get("Content-Type");
    const auto pos = content_type.find("charset");
    if (pos != std::string_view::npos) {
      return content_type.substr(0, content_type.find(";"));
    } else {
      return content_type;
    }
  }

  std::string_view charset() const noexcept {
    const auto content_type = get("Content-Type");
    const auto pos = content_type.find("charset");
    if (pos != std::string_view::npos) {
      return content_type.substr(pos + 8);
    } else {
      return content_type;
    }
  }

  std::uint64_t length() const noexcept { return content_length_; }

  ::int64_t postDataExceeded() noexcept { return postDataExceeded_; }

  bool websocket() const noexcept { return websocket_; }

  std::string_view body() const noexcept { return body_; }

  void read(char* buf, unsigned offset, unsigned size) { std::copy(body_.begin() + offset, body_.begin() + offset + size, buf);  }

  void reset() noexcept {
    data_size_ = 0;
    parse_size_ = 0;
    buffer_offset_ = 0;
    continue_parse_body_ = false;
    field_ = {};
    value_ = {};
    url_ = {};
    internalPath_.clear();
    origin_.clear();
    href_.clear();
    path_ = {};
    pathInfo_ = {};
    querystring_ = {};
    query_.clear();
    search_ = {};
    method_ = {};
    content_length_ = 0;
    postDataExceeded_ = 0;
    websocket_ = false;
    res_.reset();
    cookies_.reset();
    body_ = {};
  }

  std::pair<char*, std::size_t> buffer() noexcept {
    return {buffer_.data() + buffer_offset_, buffer_.size() - buffer_offset_};
  }

  int parse(std::size_t size) noexcept {
    data_size_ += size;
    int code{0};
    if (continue_parse_body_) {
//      body_ = {body_.data(),
//               std::min(content_length_, static_cast<std::uint64_t>(data_size_ - (body_.data() - buffer_.data())))};
      body_ = {buffer_.data() + parse_size_,
               std::min(content_length_, static_cast<std::uint64_t>(data_size_ - parse_size_))};
      //std::cout << "__________-----> " << body_ << std::endl;
      if (body_.length() < content_length_) {
        expand();
        continue_parse_body_ = true;
        code = -2;
      } else {
        continue_parse_body_ = false;
        parse_size_ += content_length_;
        if (data_size_ > parse_size_) {
          code = -3;
        } else {
          code = 0;
        }
      }
    } else {
      phr_num_headers_ = HTTP_REQUEST_HEADER_SIZE;
      if (size == 0) {
        res_.reset();
      }
      code = phr_parse_request(buffer_.data(), data_size_, &phr_method_, &phr_method_len_, &phr_path_, &phr_path_len_,
                               &phr_minor_version_, phr_headers_, &phr_num_headers_, parse_size_);
      if (code > 0) {
        // method
        parse_size_ += code;
        method_ = {phr_method_, phr_method_len_};
        // url
        url_ = {phr_path_, phr_path_len_};
        parse_url();

        // content_length
        const auto length_value = get("content-length");
        if (!length_value.empty()) {
          content_length_ = std::atoll(length_value.data());
        }

        // minor_version
        minor_version_ = phr_minor_version_;

        // websocket
        const auto upgrade = get("upgrade");
        if (!upgrade.empty()) {
          if (detail::utils::iequals(upgrade, "websocket")) {
            auto connection_value = get("connection");
            const auto key = get("sec-websocket-key");
            const auto ws_version = get("sec-websocket-version");
            if (!key.empty() && !ws_version.empty() && detail::utils::iequals(connection_value, "upgrade")) {
              websocket_ = true;
            }
          }
        }

        // cookie
        const auto cookie_string = get("cookie");
        if (!cookie_string.empty()) {
          cookies_.parse(cookie_string);
        }

        if (content_length_ > 0) {
          body_ = {buffer_.data() + code, std::min(content_length_, static_cast<std::uint64_t>(data_size_ - code))};
          if (body_.length() < content_length_) {
            if(buffer_.size() < content_length_)
                expand();
            buffer_offset_ = data_size_;
            continue_parse_body_ = true;
            code = -2;
          } else {
            parse_size_ += content_length_;
            continue_parse_body_ = false;
            if (data_size_ > parse_size_) {
                //buffer_offset_ ?
              code = -3;
            } else {
              code = 0;
            }
          }
        } else {
          if (data_size_ > parse_size_) {
            //buffer_offset_ ?
            code = -3;
          } else if (data_size_ == parse_size_) {
            code = 0;
          } else {
            expand();
            code = -2;
          }
        }
      } else if (code == -2) {
        expand();
      }
    }

    return code;
  }

//  Wt::WLocale parseLocale() const
//  {
//    return Wt::WLocale(parsePreferredAcceptValue(get("Accept-Language")));
//  }

  std::string_view parsePreferredAcceptValue() const
  {
    auto sv = get("Accept-Language");
    if (sv.empty())
      return ""sv;

    std::vector<std::string_view> lg;
    std::vector<float> quality;

    auto f = [&](auto& ctx){ quality.push_back(x3::_attr(ctx)); };
    auto defaultOption = [&](auto& /*ctx*/){ quality.push_back(1.0); };
    auto const option = ((-x3::lit('q') | -x3::lit('Q')) >> -x3::lit('=') >> x3::double_)
                            [f]
                        | (x3::alpha >> *x3::alnum >> '=' >> +x3::alnum);

    auto f2 = [&](auto& ctx){ boost::iterator_range<const char*> it = x3::_attr(ctx);  std::string_view str(it.begin(), it.size()); lg.push_back(str); std::cout << str << std::endl; };
    auto const rule = -x3::lit(',') >> x3::raw[x3::lexeme[(x3::alpha >> +(x3::alnum | x3::char_('-'))) | x3::char_('*')]][f2] >>  -((x3::lit(';') >> option) | x3::lit(',')[defaultOption]);

    x3::phrase_parse(sv.data(), sv.data() + sv.size(), x3::repeat(0, x3::inf)[rule], x3::space);

    unsigned best = 0;
    for (unsigned i = 1; i < quality.size(); ++i) {
      if (quality[i] > quality[best])
        best = i;
    }

    if (best < lg.size())
      return lg[best];

    return ""sv;
  }
  std::string_view envValue(std::string_view name) const
  {
    if (name == "CONTENT_TYPE"sv) {
      return get("Content-Type");
    } else if (name == "CONTENT_LENGTH"sv) {
      return get("Content-Length");
    } else if (name == "SERVER_SIGNATURE"sv) {
      return "<address>Wt httpd server</address>"sv;
    } else if (name == "SERVER_SOFTWARE"sv) {
      return "Wthttpd/" WT_VERSION_STR ;
    } else if (name == "SERVER_ADMIN"sv) {
      return "webmaster@localhost"sv;
    } else if (name == "REMOTE_ADDR"sv) {
      return "";//remoteAddr().c_str();
    } else if (name == "DOCUMENT_ROOT"sv) {
      return ""sv;// reply_->configuration().docRoot().c_str();
    } else
      return ""sv;
  }

  bool isPrivateIP(std::string_view address_view) const {
    auto address = boost::asio::ip::make_address(address_view);

    if (address.is_v4()) {
      auto v4Address = address.to_v4();
      unsigned long ip = v4Address.to_ulong();
      return ((ip >= 0x0A000000 && ip <= 0x0AFFFFFF) ||
              (ip >= 0xAC100000 && ip <= 0xAC1FFFFF) ||
              (ip >= 0xC0A80000 && ip <= 0xC0A8FFFF));
    }
    else if (address.is_v6()) {
      auto v6Address = address.to_v6();
      return v6Address.is_site_local();
    }

    return false;
  }

  ::int64_t postDataExceeded_{0};
 private:

  void parse_url() {
    urlv_= boost::url_view { url_ };

    path_ = urlv_.encoded_path();
    querystring_ = urlv_.encoded_query();

//        const auto pos = url_.find('?');
//        if (pos == std::string_view::npos) {
//          path_ = url_;
//        } else {
//          path_ = url_.substr(0, pos);
//          querystring_ = url_.substr(pos + 1, url_.length() - pos - 1);
//          search_ = url_.substr(pos, url_.length() - pos);
//        }
  }
  void setInternalPath(std::string_view internalPath) { internalPath_ = internalPath; }

  void expand() {
    const char* data{buffer_.data()};
    buffer_offset_ = data_size_;//buffer_.size();
    buffer_.resize(buffer_.size() * 2);

    for (std::size_t i{0}; i < phr_num_headers_; ++i) {
      auto& header = phr_headers_[i];
      header.name = buffer_.data() + (header.name - data);
      header.value = buffer_.data() + (header.value - data);
    }

    if (!method_.empty()) {
      method_ = {buffer_.data() + (method_.data() - data), method_.length()};
    }

    if (!url_.empty()) {
      url_ = {buffer_.data() + (url_.data() - data), url_.length()};
      urlv_= boost::url_view { url_ };
    }

    if (!path_.empty()) {
      path_ = {buffer_.data() + (path_.data() - data), path_.length()};
    }

    if (!querystring_.empty()) {
      querystring_ = {buffer_.data() + (querystring_.data() - data), querystring_.length()};
    }

    if (!body_.empty()) {
      body_ = {buffer_.data() + (body_.data() - data), body_.length()};
    }
  }

  static constexpr std::size_t HTTP_REQUEST_BUFFER_SIZE{4096};
  static constexpr std::size_t HTTP_REQUEST_HEADER_SIZE{64};
  bool https_{false};
  std::vector<char> buffer_;
  std::size_t data_size_{0};
  std::size_t parse_size_{0};
  std::size_t buffer_offset_{0};
  const char* phr_method_{nullptr};
  const char* phr_path_{nullptr};
  int phr_minor_version_;
  phr_header phr_headers_[HTTP_REQUEST_HEADER_SIZE];
  std::size_t phr_method_len_;
  std::size_t phr_path_len_;
  std::size_t phr_num_headers_;
  bool continue_parse_body_{false};
  std::string_view field_;
  std::string_view value_;
  unsigned minor_version_{1};
  std::string_view url_;
  boost::url_view urlv_;
  std::string internalPath_;
  mutable std::string origin_;
  mutable std::string href_;
  std::string_view path_, pathInfo_;
  std::string_view querystring_;
  mutable std::multimap<std::string, std::string> query_;
  std::string_view search_;
  std::string_view method_;
  std::uint64_t content_length_{0};
  bool websocket_{false};
  response& res_;
  cookies& cookies_;
  std::string_view body_;

  ParameterMap parameters_;
  UploadedFileMap files_;
};

}  // namespace http
}  // namespace cue

#endif  // CUEHTTP_REQUEST_HPP_
