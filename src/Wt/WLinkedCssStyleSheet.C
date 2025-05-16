/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include "Wt/WApplication.h"
#include "Wt/WLinkedCssStyleSheet.h"
#include "Wt/WStringStream.h"

namespace Wt {

WLinkedCssStyleSheet::WLinkedCssStyleSheet(const WLink& link, const std::string& media)
  : link_(link),
    media_(media)
{ }

void WLinkedCssStyleSheet::cssText(WStringStream& out) const
{
  WApplication *app = WApplication::instance();
  out << "@import url(\"" << link_.resolveUrl(app) << "\")";

  if (!media_.empty() && media_ != "all")
    out << " " << media_;
  out << ";\n";
}

void WLinkedCssStyleSheet::cssText(fmt::memory_buffer &out) const
{
  WApplication *app = WApplication::instance();
  std::string resolved_url = link_.resolveUrl(app);
  //std::string media_attribute;

  fmt::format_to(std::back_inserter(out), FMT_COMPILE("@import url(\"{}\") {};\n"), resolved_url, media_ != "all" ? media_ : "");

  // Construct the media attribute string only if needed
  // if (!media_.empty() && media_ != "all") {
  //     // Ensure media attribute value is properly escaped if necessary, though typically not needed for standard media queries.
  //     //media_attribute = fmt::format(FMT_COMPILE(" media=\"{}\""), media_); // Format as ' media="value"'
  // }

  // Format the <link> tag
  // fmt::format_to(std::back_inserter(out),
  //                FMT_COMPILE("<link rel=\"stylesheet\" href=\"{}\" type=\"text/css\"{}>\n"),
  //                resolved_url,
  //                media_attribute); // Insert the media attribute string (or empty string)
}

} // namespace Wt
