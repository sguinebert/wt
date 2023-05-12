// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#ifndef CGI_PARSER_H_
#define CGI_PARSER_H_

#include <string>
#include <map>
#include <iostream>
#include <vector>

#include <Wt/WDllDefs.h>
#include <Wt/cuehttp/context.hpp>

namespace Wt {

class CgiParser;
class WebRequest;

/*
 * Parses CGI in all its forms (get/post/file uploads).
 *
 * (WT_API added for tests)
 */
class WT_API CgiParser
{
public:
  enum ReadOption { ReadDefault, ReadHeadersOnly, ReadBodyAnyway };

  static void init();

  CgiParser(::int64_t maxRequestSize, ::int64_t maxFormData);

  /*
   * Reads in GET or POST data, converts it to unescaped text, and
   * creates Entry for each parameter entry. The request is annotated
   * with the parse results.
   */
  void parse(WebRequest& request, ReadOption option);
  void parse(Wt::http::context* context, ReadOption option);
  void parse(std::string_view message, std::string_view sessionID, Wt::http::context* context, ReadOption option);

private:
  void readMultipartData(Wt::http::context* context, std::string_view type, ::int64_t len);
    bool parseBody(Wt::http::context* context, const std::string boundary);
    bool parseHead(Wt::http::context* context);

  void readMultipartData(WebRequest& request, const std::string type, ::int64_t len);
  bool parseBody(WebRequest& request, const std::string boundary);
  bool parseHead(WebRequest& request);
  ::int64_t maxFormData_, maxRequestSize_, left_;
  std::ostream *spoolStream_;
  WebRequest *request_;

  std::string currentKey_;

  void readUntilBoundary(Wt::http::context* context, const std::string boundary,
                         int tossAtBoundary,
                         std::string *resultString,
                         std::ostream *resultFile);

  void readUntilBoundary(WebRequest& request, const std::string boundary,
                         int tossAtBoundary,
                         std::string *resultString,
                         std::ostream *resultFile);
  void windBuffer(int offset);
  int index(const std::string search);

  enum {BUFSIZE = 8192};
  enum {MAXBOUND = 100};

  int buflen_;
  char buf_[BUFSIZE + (int)MAXBOUND];
};

}

#endif // CGI_PARSER_H_
