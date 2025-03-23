/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 * In addition to these terms, permission is also granted to use and
 * modify these two files (CgiParser.C and CgiParser.h) so long as the
 * copyright above is maintained, modifications are documented, and
 * credit is given for any use of the library.
 *
 * CGI parser modelled after the PERL implementation cgi-lib.pl 2.18 by
 * Steven E. Brenner with the following original copyright:

# Perl Routines to Manipulate CGI input
# cgi-lib@pobox.com
#
# Copyright (c) 1993-1999 Steven E. Brenner  
# Unpublished work.
# Permission granted to use and modify this library so long as the
# copyright above is maintained, modifications are documented, and
# credit is given for any use of the library.
#
# Thanks are due to many people for reporting bugs and suggestions

# For more information, see:
#     http://cgi-lib.stanford.edu/cgi-lib/  

 */

#include <fstream>
#include <stdlib.h>

#ifdef WT_HAVE_GNU_REGEX
#include <regex.h>
#else
#include <regex>
#endif // WT_HAVE_GNU_REGEX

#include "CgiParser.h"
#include "WebRequest.h"
#include "WebUtils.h"
#include "FileUtils.h"

#include "Wt/WException.h"
#include "Wt/WLogger.h"
#include "Wt/Http/Request.h"

using std::memmove;
using std::strcpy;
using std::strtol;

namespace
{
void parseStringView(std::string_view input, std::vector<std::pair<std::string_view, std::string_view>>& parsed) {
    if(input[0] == '&') {
        input = input.substr(1);
    }

    char* data = (char *)input.data();
    size_t size = input.size();
    unsigned tail = size - size % 32;
    bool isKey = true;

    __m256i separatorsAnd = _mm256_set1_epi8('&');
    __m256i separatorsEqual = _mm256_set1_epi8('=');

    int remaining = 0;
    // Process 32 bytes at a time (256 bits)
    for (size_t i = 0; i < tail; i += 32) {
        char *ptr = data + i;
        __m256i inputChunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));

        // Find positions of '&' and '=' using SIMD comparison
        __m256i cmpAnd = _mm256_cmpeq_epi8(inputChunk, separatorsAnd);
        __m256i cmpEqual = _mm256_cmpeq_epi8(inputChunk, separatorsEqual);

        // Mask indicating positions where '&' or '=' occur
        __m256i mask = _mm256_or_si256(cmpAnd, cmpEqual);

        // Extract mask bits as a 32-bit integer
        unsigned maskBits = _mm256_movemask_epi8(mask);

        if(!maskBits)
            remaining += 32;
        // Process each position separately
        while (maskBits != 0) {
            // Find the position of the first separator
            int position = __builtin_ctz(maskBits);

            // Create sub string_view for the parsed segment
            std::string_view segment(ptr - remaining, position + remaining);
            if(isKey) {
                parsed.emplace_back(segment, std::string_view());
                isKey = false;
            }
            else {
                parsed.back().second = segment;
                isKey = true;
            }

            // Move to the next segment
            ptr += position + 1;

            // Update the mask by shifting out the processed bits
            maskBits >>= position + 1;
            if(!maskBits)
                remaining = data + i + 32 - ptr;
            else
                remaining = 0;
        }
    }

    for(unsigned pos = tail; pos < size;)
    {
        std::size_t next = input.find_first_of("&=", pos);

        if (next == pos && input[next] == '&')
        {
            if(!isKey) {
                parsed.back().second = input.substr(pos - remaining, next - pos + remaining);
                isKey = true;
                remaining = 0;
            }
            // skip empty
            pos = next + 1;
            continue;
        }

        if (next == std::string::npos || input[next] == '&')
        {
            if (next == std::string::npos)
                next = input.length();
            if(!isKey) {
                parsed.back().second = input.substr(pos - remaining, next - pos + remaining);
                isKey = true;
            }
            else
                parsed.emplace_back(input.substr(pos, next - pos), std::string_view());
            pos = next + 1;
        }
        else
        {
            std::size_t amp = input.find('&', next + 1);
            if (amp == std::string::npos)
                amp = input.length();

            parsed.emplace_back(input.substr(pos - remaining, next - pos + remaining), input.substr(next + 1, amp - (next + 1)));
            pos = amp + 1;
        }
        remaining = 0;
    }
}

using svmatch = std::match_results<std::string_view::const_iterator>;
using svsub_match = std::sub_match<std::string_view::const_iterator>;

inline std::string_view get_sv(const svsub_match& m) {
    return std::string_view(m.first, m.length());
}

inline bool regex_match(std::string_view sv,
                        svmatch& m,
                        const std::regex& e,
                        std::regex_constants::match_flag_type flags =
                        std::regex_constants::match_default) {
    return std::regex_match(sv.begin(), sv.end(), m, e, flags);
}

inline bool regex_match(std::string_view sv,
                        const std::regex& e,
                        std::regex_constants::match_flag_type flags =
                        std::regex_constants::match_default) {
    return std::regex_match(sv.begin(), sv.end(), e, flags);
}

inline bool regex_search(std::string_view sv,
                        svmatch &what,
                        const std::regex& e,
                        std::regex_constants::match_flag_type flags =
                        std::regex_constants::match_default) {
    return std::regex_search(sv.begin(), sv.end(), what, e, flags);
}


#ifndef WT_HAVE_GNU_REGEX
  const std::regex boundary_e("\\bboundary=(?:(?:\"([^\"]+)\")|(\\S+))",
                              std::regex::icase);
  const std::regex name_e("\\bname=(?:(?:\"([^\"]+)\")|([^\\s:;]+))",
                          std::regex::icase);
  const std::regex filename_e("\\bfilename=(?:(?:\"([^\"]*)\")|([^\\s:;]+))",
                              std::regex::icase);
  const std::regex content_e("^\\s*Content-type:"
                             "\\s*(?:(?:\"([^\"]+)\")|([^\\s:;]+))",
                             std::regex::icase);
  const std::regex content_disposition_e("^\\s*Content-Disposition:",
                                         std::regex::icase);
  const std::regex content_type_e("^\\s*Content-Type:",
                                  std::regex::icase);

  bool fishValue(const std::string &text,
                 const std::regex &e, std::string &result)
  {
    std::smatch what;

    if (std::regex_search(text, what, e))
    {
      result = std::string(what[1]) + std::string(what[2]);
      return true;
    }
    else
      return false;
  }

  bool fishValue(std::string_view text,
                 const std::regex &e, std::string &result)
  {
    svmatch what;

    if (regex_search(text, what, e))
    {
      result = std::string(what[1]) + std::string(what[2]);
      return true;
    }
    else
      return false;
  }

  bool regexMatch(const std::string &text, const std::regex &e)
  {
    return std::regex_search(text, e);
  }

#else
  regex_t boundary_e, name_e, filename_e, content_e,
      content_disposition_e, content_type_e;

  const char *boundary_ep = "\\bboundary=((\"([^\"]*)\")|([^ \t]*))";
  const char *name_ep = "\\bname=((\"([^\"]*)\")|([^ \t:;]*))";
  const char *filename_ep = "\\bfilename=((\"([^\"]*)\")|([^ \t:;]*))";
  const char *content_ep = "^[ \t]*Content-type:"
                           "[ \t]*((\"([^\"]*)\")|([^ \t:;]*))";
  const char *content_disposition_ep = "^[ \t]*Content-Disposition:";
  const char *content_type_ep = "^[ \t]*Content-Type:";

  bool fishValue(const std::string &text,
                 regex_t &e1, std::string &result)
  {
    regmatch_t pmatch[5];
    int res = regexec(&e1, text.c_str(), 5, pmatch, 0);

    if (res == 0)
    {
      if (pmatch[3].rm_so != -1)
        result = text.substr(pmatch[3].rm_so,
                             pmatch[3].rm_eo - pmatch[3].rm_so);
      if (pmatch[4].rm_so != -1)
        result = text.substr(pmatch[4].rm_so,
                             pmatch[4].rm_eo - pmatch[4].rm_so);

      return true;
    }
    else
      return false;
  }

  bool regexMatch(const std::string &text, regex_t &e)
  {
    regmatch_t pmatch[1];

    return regexec(&e, text.c_str(), 1, pmatch, 0) == 0;
  }

  class RegInitializer
  {
  protected:
    static bool regInitialized_;

  public:
    RegInitializer()
    {
    }

    ~RegInitializer()
    {
      cleanup();
    }

    static void setup()
    {
      if (!regInitialized_)
      {
        regcomp(&boundary_e, boundary_ep, REG_ICASE | REG_EXTENDED);
        regcomp(&name_e, name_ep, REG_ICASE | REG_EXTENDED);
        regcomp(&filename_e, filename_ep, REG_ICASE | REG_EXTENDED);
        regcomp(&content_e, content_ep, REG_ICASE | REG_EXTENDED);
        regcomp(&content_disposition_e, content_disposition_ep,
                REG_ICASE | REG_EXTENDED);
        regcomp(&content_type_e, content_type_ep, REG_ICASE | REG_EXTENDED);
        regInitialized_ = true;
      }
    }

    static void cleanup()
    {
      if (regInitialized_)
      {
        regfree(&boundary_e);
        regfree(&name_e);
        regfree(&filename_e);
        regfree(&content_e);
        regfree(&content_disposition_e);
        regfree(&content_type_e);
        regInitialized_ = false;
      }
    }
  };

  bool RegInitializer::regInitialized_ = false;

  static RegInitializer regInitializer;
#endif
}

namespace Wt
{

  LOGGER("CgiParser");

  void CgiParser::init()
  {
#ifdef WT_HAVE_GNU_REGEX
    RegInitializer::setup();
#endif
  }

  CgiParser::CgiParser(::int64_t maxRequestSize, ::int64_t maxFormData)
      : maxFormData_(maxFormData),
        maxRequestSize_(maxRequestSize)
  {
  }

  void CgiParser::parse(WebRequest &request, ReadOption readOption)
  {
    /*
    * TODO: optimize this ...
    */
    request_ = &request;

    ::int64_t len = request.contentLength();
    const char *type = request.contentType();
    const char *meth = request.requestMethod();

    request.postDataExceeded_ = (len > maxRequestSize_ ? len : 0);

    std::string queryString = request.queryString();

    LOG_DEBUG("queryString (len={}): {}", len, queryString);

    if (!queryString.empty() && request_->parameters_.empty())
    {
      Http::Request::parseFormUrlEncoded(queryString, request_->parameters_);
    }

    // XDomainRequest cannot set a contentType header, we therefore pass it
    // as a request parameter
    if (readOption != ReadHeadersOnly &&
        strcmp(meth, "POST") == 0 &&
        ((type && strstr(type, "application/x-www-form-urlencoded") == type) ||
         (queryString.find("&contentType=x-www-form-urlencoded") != std::string::npos)))
    {
      /*
      * TODO: parse this stream-based to avoid the malloc here. For now
      * we protect the maximum that can be POST'ed as form data.
      */
      if (len > maxFormData_)
        throw WException("Oversized application/x-www-form-urlencoded (" + std::to_string(len) + ")");

      auto buf = std::unique_ptr<char[]>(new char[len + 1]);

      request.in().read(buf.get(), len);

      if (request.in().gcount() != (int)len)
      {
        throw WException("Unexpected short read.");
      }

      buf[len] = 0;

      // This is a special Wt feature, I do not think it standard.
      // For POST, parameters in url-encoded URL are still parsed.

      std::string formQueryString = buf.get();

      // if(request.isWebSocketMessage())
      //   std::cout << "formQueryString (len=" << len << "): " << formQueryString << std::endl;

      LOG_DEBUG("formQueryString (len={}): {}", len, formQueryString);
      if (!formQueryString.empty())
      {
        Http::Request::parseFormUrlEncoded(formQueryString, request_->parameters_);
      }
      Http::ParameterMap::const_iterator it = request_->parameters_.find("Wt-params");
      if (it != request_->parameters_.end() && it->second.size() == 1)
      {
        Http::Request::parseFormUrlEncoded(it->second[0], request_->parameters_);
      }
    }

    if (readOption != ReadHeadersOnly &&
        type && strstr(type, "multipart/form-data") == type)
    {
      if (strcmp(meth, "POST") != 0)
      {
        throw WException("Invalid method for multipart/form-data: " + std::string(meth));
      }

      if (!request.postDataExceeded_)
        readMultipartData(request, type, len);
      else if (readOption == ReadBodyAnyway)
      {
        for (; len > 0;)
        {
          ::int64_t toRead = std::min(::int64_t(BUFSIZE), len);
          request.in().read(buf_, toRead);
          if (request.in().gcount() != (::int64_t)toRead)
            throw WException("CgiParser: short read");
          len -= toRead;
        }
      }
    }
  }

  void CgiParser::parse(Wt::http::context *context, ReadOption readOption)
  {
    /*
    * TODO: optimize this ...
    */
    //request_ = &request;
    auto& request = context->req();

    ::int64_t len = request.length();
    auto type = request.type();
    auto meth = request.method();

    context->req().postDataExceeded_ = (len > maxRequestSize_ ? len : 0);

    auto queryString = request.querystring();

    LOG_DEBUG("queryString (len={}): {}", len, queryString);

//    if (!queryString.empty() && request.query().empty())
//    {
//      Http::Request::parseFormUrlEncoded(queryString, request.query());
//    }

    // XDomainRequest cannot set a contentType header, we therefore pass it
    // as a request parameter
    if (readOption != ReadHeadersOnly &&
        meth == "POST" &&
        ((!type.empty() && type.find("application/x-www-form-urlencoded") != std::string_view::npos) ||
         (queryString.find("&contentType=x-www-form-urlencoded") != std::string_view::npos)))
    {
      /*
          * TODO: parse this stream-based to avoid the malloc here. For now
          * we protect the maximum that can be POST'ed as form data.
          */
      if (len > maxFormData_)
        throw WException("Oversized application/x-www-form-urlencoded (" + std::to_string(len) + ")");

//      auto buf = std::unique_ptr<char[]>(new char[len + 1]);

//      auto view = context->req().body();

//      std::copy(view.begin(), view.end(), buf.get());
//      buf[len] = '\0'; // Null-terminate the string


      //          request.in().read(buf.get(), len);
      //          if (request.in().gcount() != (int)len)
      //          {
      //              throw WException("Unexpected short read.");
      //          }

      //buf[len] = 0;

      // This is a special Wt feature, I do not think it standard.
      // For POST, parameters in url-encoded URL are still parsed.

      //std::string formQueryString = buf.get();
      auto message = request.body();
      auto& parameters = context->req().query();

      // if(request.isWebSocketMessage())
      //   std::cout << "formQueryString (len=" << len << "): " << formQueryString << std::endl;
      //std::cerr << fmt::format("formQueryString (len={}): {} type : {}", len, fsv, type) << std::endl;

      LOG_DEBUG("formQueryString (len={}): {}", len, queryString);
      if (!message.empty())
      {
        std::vector<std::pair<std::string_view, std::string_view>> views;
        parseStringView(message, views);
        request.parseFormUrlEncoded(views);
        //request.parseFormUrlEncoded(message);
      }

      if (auto [begin, end] = parameters.equal_range("Wt-params"); begin != parameters.end() && std::distance(begin, end) == 1)
      {
          request.parseFormUrlEncoded(begin->second);
      }
//      for(auto parameter : parameters) {
//          std::cerr << "parameter : " << parameter.first << " - " << parameter.second << std::endl;
//      }
//      if (!formQueryString.empty())
//      {
//        //Http::Request::parseFormUrlEncoded(formQueryString, request.query());
//      }

//      if (auto it = request.query().find("Wt-params"); it != request.query().end() && it->second.size() == 1)
//      {
//        //Http::Request::parseFormUrlEncoded(it->second[0], request.query());
//      }
    }

    if (readOption != ReadHeadersOnly &&
        !type.empty() && type.find("multipart/form-data") != std::string_view::npos)
    {
      std::cerr << fmt::format("multipart/form-data (len={}): {} type : {}", len, "ReadHeadersOnly", type) << std::endl;
      if (meth != "POST")
      {
        throw WException("Invalid method for multipart/form-data: " + std::string(meth));
      }

      if (!context->postDataExceeded())
        readMultipartData(context, type, len);
      else if (readOption == ReadBodyAnyway)
      {
        unsigned offset = 0;
        for (; len > 0;)
        {
          ::int64_t toRead = std::min(::int64_t(BUFSIZE), len);
          context->req().read(buf_, offset, toRead);
//          if (context->req().gcount() != (::int64_t)toRead)
//            throw WException("CgiParser: short read");
          len -= toRead;
          offset += toRead;
        }
      }
    }
  }

  void CgiParser::parse(std::string_view message, std::string_view sessionID, http::context *context, ReadOption option)
  {
    /*
    * TODO: optimize this ...
    */
    //request_ = &request;
    auto& request = context->req();

    auto session = context->websession();

    auto& parameters = context->req().query();

    //auto& parameters = context->req().getParameters();

    parameters.emplace("wtd", std::string(sessionID));
    parameters.emplace("request", "jsupdate");

//    parameters["wtd"].push_back(sessionID);
//    parameters["request"].push_back("jsupdate");


    ::int64_t len = message.length();
//    std::string_view type = "application/x-www-form-urlencoded";
//    std::string_view meth = "POST";
    //auto queryString_ = "wtd=" + session_->sessionId() + "&request=jsupdate";
    //auto queryString = context->ws_querystring();

    context->req().postDataExceeded_ = (len > maxRequestSize_ ? len : 0);

    LOG_DEBUG("queryString (len={}): {}", len, message);

    //    if (!queryString.empty() && request.query().empty())
    //    {
    //      Http::Request::parseFormUrlEncoded(queryString, request.query());
    //    }

    /* XDomainRequest cannot set a contentType header, we therefore pass it  as a request parameter */
    if (option != ReadHeadersOnly /*&&
        meth == "POST" &&
        ((!type.empty() && type.find("application/x-www-form-urlencoded") != std::string_view::npos)||
         (queryString.find("&contentType=x-www-form-urlencoded") != std::string_view::npos))*/)
    {
         /*
          * TODO: parse this stream-based to avoid the malloc here. For now
          * we protect the maximum that can be POST'ed as form data.
          */
      if (len > maxFormData_)
        throw WException("Oversized application/x-www-form-urlencoded (" + std::to_string(len) + ")");

      //      auto buf = std::unique_ptr<char[]>(new char[len + 1]);

      //      auto view = context->req().body();

      //      std::copy(view.begin(), view.end(), buf.get());
      //      buf[len] = '\0'; // Null-terminate the string


      //          request.in().read(buf.get(), len);
      //          if (request.in().gcount() != (int)len)
      //          {
      //              throw WException("Unexpected short read.");
      //          }

      //buf[len] = 0;

      // This is a special Wt feature, I do not think it standard.
      // For POST, parameters in url-encoded URL are still parsed.

      //std::string formQueryString = buf.get();

      // if(request.isWebSocketMessage())
      //   std::cout << "formQueryString (len=" << len << "): " << formQueryString << std::endl;

      LOG_DEBUG("formQueryString (len={}): {}", len, message);
      if (!message.empty())
      {
        std::vector<std::pair<std::string_view, std::string_view>> views;
        parseStringView(message, views);
        request.parseFormUrlEncoded(views);
        //request.parseFormUrlEncoded(message);
      }

      // for(auto &[key, val]: parameters) {
      //     std::cout << "param [k, v]: " << key << " - "<< val << std::endl;
      // }

      if (auto [begin, end] = parameters.equal_range("Wt-params"); begin != parameters.end() && std::distance(begin, end) == 1)
      {
          request.parseFormUrlEncoded(begin->second);
      }

//      if (auto it = parameters.find("Wt-params"); it != parameters.end() && it->second.size() == 1)
//      {
//          request.parseFormUrlEncoded(it->second[0], parameters);
//      }
    }

  }

  void CgiParser::readMultipartData(Wt::http::context *context, std::string_view type, int64_t len)
  {
    std::string boundary;

    if (!fishValue(type, boundary_e, boundary))
      throw WException("Could not find a boundary for multipart data.");

    boundary = "--" + boundary;

    buflen_ = 0;
    left_ = len;
    spoolStream_ = 0;
    currentKey_.clear();

    if (!parseBody(context, boundary))
      return;

    for (;;)
    {
      if (!parseHead(context))
        break;
      if (!parseBody(context, boundary))
        break;
    }
  }

  bool CgiParser::parseBody(Wt::http::context *context, const std::string boundary)
  {
    std::string value;

    readUntilBoundary(context, boundary, 2,
                      spoolStream_ ? 0 : (!currentKey_.empty() ? &value : 0),
                      spoolStream_);

    if (spoolStream_)
    {
      LOG_DEBUG("completed spooling");
      delete spoolStream_;
      spoolStream_ = 0;
    }
    else
    {
      if (!currentKey_.empty())
      {
        LOG_DEBUG("value: \"{}\"", value);
        context->req().query().emplace(currentKey_, value);
        //context->req().query().
        //request_->parameters_[currentKey_].push_back(value);
      }
    }

    currentKey_.clear();

    if (std::string(buf_ + boundary.length(), 2) == "--")
      return false;

    windBuffer(boundary.length() + 2);

    return true;
  }

  bool CgiParser::parseHead(Wt::http::context *context)
  {
    std::string head;
    readUntilBoundary(context, "\r\n\r\n", -2, &head, 0);

    std::string name;
    std::string fn;
    std::string ctype;

    for (unsigned current = 0; current < head.length();)
    {
      /* read line by line */
      std::string::size_type i = head.find("\r\n", current);
      const std::string text = head.substr(current, (i == std::string::npos
                                                         ? std::string::npos
                                                         : i - current));

      if (regexMatch(text, content_disposition_e))
      {
        fishValue(text, name_e, name);
        fishValue(text, filename_e, fn);
      }

      if (regexMatch(text, content_type_e))
      {
        fishValue(text, content_e, ctype);
      }

      current = i + 2;
    }

    LOG_DEBUG("name: {} ct: {} fn: {}", name, ctype, fn);

    currentKey_ = name;

    if (!fn.empty())
    {
      if (!context->postDataExceeded())
      {
        /*
       * It is not easy to create a std::ostream pointing to a
       * temporary file name.
       */
        std::string spool = FileUtils::createTempFileName();

        spoolStream_ = new std::ofstream(spool.c_str(),
                                         std::ios::out | std::ios::binary);

        context->req().files().insert(std::make_pair(name, Http::UploadedFile(spool, fn, ctype)));

        LOG_DEBUG("spooling file to {}", spool.c_str());
      }
      else
      {
        spoolStream_ = 0;
        // Clear currentKey so that file we don't do harm by reading this
        // giant blob in memory
        currentKey_ = "";
      }
    }

    windBuffer(4);

    return true;
  }

  void CgiParser::readMultipartData(WebRequest &request,
                                    const std::string type, ::int64_t len)
  {
    std::string boundary;

    if (!fishValue(type, boundary_e, boundary))
      throw WException("Could not find a boundary for multipart data.");

    boundary = "--" + boundary;

    buflen_ = 0;
    left_ = len;
    spoolStream_ = 0;
    currentKey_.clear();

    if (!parseBody(request, boundary))
      return;

    for (;;)
    {
      if (!parseHead(request))
        break;
      if (!parseBody(request, boundary))
        break;
    }
  }

  /*
  * Read until finding the boundary, saving to resultString or
  * resultFile. The boundary itself is not consumed.
  *
  * tossAtBoundary controls how many characters extra (<0)
  * or few (>0) are saved at the start of the boundary in the result.
  */
  void CgiParser::readUntilBoundary(WebRequest &request,
                                    const std::string boundary,
                                    int tossAtBoundary,
                                    std::string *resultString,
                                    std::ostream *resultFile)
  {
    int bpos;

    while ((bpos = index(boundary)) == -1)
    {
      /*
      * If we couldn't find it. We need to wind the buffer, but only save
      * not including the boundary length.
      */
      if (left_ == 0)
        throw WException("CgiParser: reached end of input while seeking end of "
                         "headers or content. Format of CGI input is wrong");

      /* save (up to) BUFSIZE from buffer to file or value string, but
      * mind the boundary length */
      int save = std::min((buflen_ - (int)boundary.length()), (int)BUFSIZE);

      if (save > 0)
      {
        if (resultString)
          *resultString += std::string(buf_, save);
        if (resultFile)
          resultFile->write(buf_, save);

        /* wind buffer */
        windBuffer(save);
      }

      unsigned amt = static_cast<unsigned>(std::min(left_,
                                                    static_cast<::int64_t>(BUFSIZE + (int)MAXBOUND - buflen_)));

      request.in().read(buf_ + buflen_, amt);
      if (request.in().gcount() != (int)amt)
        throw WException("CgiParser: short read");

      left_ -= amt;
      buflen_ += amt;
    }

    if (resultString)
      *resultString += std::string(buf_, bpos - tossAtBoundary);
    if (resultFile)
      resultFile->write(buf_, bpos - tossAtBoundary);

    /* wind buffer */
    windBuffer(bpos);
  }

  void CgiParser::windBuffer(int offset)
  {
    if (offset < buflen_)
    {
      memmove(buf_, buf_ + offset, buflen_ - offset);
      buflen_ -= offset;
    }
    else
      buflen_ = 0;
  }

  int CgiParser::index(std::string_view search)
  {
    std::string_view bufS(buf_, buflen_);

    auto i = bufS.find(search);

    if (i == std::string_view::npos)
      return -1;
    else
      return i;
  }

  int CgiParser::index(std::string_view source, std::string_view search)
  {
    auto i = source.find(search);

    if (i == std::string_view::npos)
      return -1;
    else
      return i;
  }

  bool CgiParser::parseHead(WebRequest &request)
  {
    std::string head;
    readUntilBoundary(request, "\r\n\r\n", -2, &head, 0);

    std::string name;
    std::string fn;
    std::string ctype;

    for (unsigned current = 0; current < head.length();)
    {
      /* read line by line */
      std::string::size_type i = head.find("\r\n", current);
      const std::string text = head.substr(current, (i == std::string::npos
                                                         ? std::string::npos
                                                         : i - current));

      if (regexMatch(text, content_disposition_e))
      {
        fishValue(text, name_e, name);
        fishValue(text, filename_e, fn);
      }

      if (regexMatch(text, content_type_e))
      {
        fishValue(text, content_e, ctype);
      }

      current = i + 2;
    }

    LOG_DEBUG("name: {} ct: {} fn: {}", name, ctype, fn);

    currentKey_ = name;

    if (!fn.empty())
    {
      if (!request.postDataExceeded_)
      {
        /*
       * It is not easy to create a std::ostream pointing to a
       * temporary file name.
       */
        std::string spool = FileUtils::createTempFileName();

        spoolStream_ = new std::ofstream(spool.c_str(),
                                         std::ios::out | std::ios::binary);

        request_->files_.insert(std::make_pair(name, Http::UploadedFile(spool, fn, ctype)));

        LOG_DEBUG("spooling file to {}", spool.c_str());
      }
      else
      {
        spoolStream_ = 0;
        // Clear currentKey so that file we don't do harm by reading this
        // giant blob in memory
        currentKey_ = "";
      }
    }

    windBuffer(4);

    return true;
  }

  void CgiParser::readUntilBoundary(Wt::http::context *context,
                                    const std::string boundary,
                                    int tossAtBoundary,
                                    std::string *resultString,
                                    std::ostream *resultFile)
  {
//    auto body = context->req().body();

//    if(auto bpos = body.find(boundary); bpos != std::string_view::npos) {
//      if(auto epos = body.find(boundary, bpos + boundary.size()); epos != std::string_view::npos) {
//        auto extract = body.substr(bpos + boundary.size(), body.size() - bpos - boundary.size() - epos);
//        if (resultString)
//          *resultString += std::string(extract);
//        if (resultFile)
//          resultFile->write(extract.data(), extract.size());
//        return;
//      }
//    }
//    throw WException("CgiParser: reached end of input while seeking end of "
//                     "headers or content. Format of CGI input is wrong");


    int bpos;

    while ((bpos = index(boundary)) == -1)
    {
      /*
      * If we couldn't find it. We need to wind the buffer, but only save
      * not including the boundary length.
      */
      if (left_ == 0)
        throw WException("CgiParser: reached end of input while seeking end of "
                         "headers or content. Format of CGI input is wrong");

      /* save (up to) BUFSIZE from buffer to file or value string, but
      * mind the boundary length */
      int save = std::min((buflen_ - (int)boundary.length()), (int)BUFSIZE);

      if (save > 0)
      {
        if (resultString)
          *resultString += std::string(buf_, save);
        if (resultFile)
          resultFile->write(buf_, save);

        /* wind buffer */
        windBuffer(save);
      }

      unsigned amt = static_cast<unsigned>(std::min(left_,
                                                    static_cast<::int64_t>(BUFSIZE + (int)MAXBOUND - buflen_)));


      context->req().read(buf_ + buflen_, offset_, amt);

      left_ -= amt;
      buflen_ += amt;
      offset_ += amt;
    }

    if (resultString)
      *resultString += std::string(buf_, bpos - tossAtBoundary);
    if (resultFile)
      resultFile->write(buf_, bpos - tossAtBoundary);

    /* wind buffer */
    windBuffer(bpos);
  }

  bool CgiParser::parseBody(WebRequest &request, const std::string boundary)
  {
    std::string value;

    readUntilBoundary(request, boundary, 2,
                      spoolStream_ ? 0 : (!currentKey_.empty() ? &value : 0),
                      spoolStream_);

    if (spoolStream_)
    {
      LOG_DEBUG("completed spooling");
      delete spoolStream_;
      spoolStream_ = 0;
    }
    else
    {
      if (!currentKey_.empty())
      {
        LOG_DEBUG("value: \"{}\"", value);
        request_->parameters_[currentKey_].push_back(value);
      }
    }

    currentKey_.clear();

    if (std::string(buf_ + boundary.length(), 2) == "--")
      return false;

    windBuffer(boundary.length() + 2);

    return true;
  }

} // namespace Wt
