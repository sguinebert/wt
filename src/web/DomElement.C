/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#include <cstdio>
#include <sstream>
#include <ranges>

#include "Wt/WObject.h"
#include "Wt/WApplication.h"
#include "Wt/WContainerWidget.h"
#include "Wt/WEnvironment.h"
#include "Wt/WException.h"
#include "Wt/WStringStream.h"
#include "Wt/WTheme.h"

#include "DomElement.h"
#include "WebUtils.h"
#include "StringUtils.h"

namespace {

std::string elementNames_[] =
  { "a", "br", "button", "col",
    "colgroup",
    "div", "fieldset", "form",
    "h1", "h2", "h3", "h4",

    "h5", "h6", "iframe", "img",
    "input", "label", "legend", "li",
    "ol",

    "option", "ul", "script", "select",
    "span", "table", "tbody", "thead",
    "tfoot", "th", "td", "textarea",
    "optgroup",

    "tr", "p", "canvas",
    "map", "area", "style",

    "object", "param",

    "audio", "video", "source",

    "b", "strong", "em", "i", "hr"
  };

bool defaultInline_[] =
  { true, false, true, false,
    false,
    false, false, false,
    false, false, false, false,

    false, false, true, true,
    true, true, true, false,
    false,

    true, false, false, true,
    true, false, false, false,
    false, false, false, true,
    true,

    false, false, true,
    false, true, true,

    false, false,

    false, false, false,

    true, true, true, true, false
  };
// CSS property names
std::string cssNames_[] =
  { "position",
    "z-index", "float", "clear",
    "width", "height", "line-height",
    "min-width", "min-height",
    "max-width", "max-height",
    "left", "right", "top", "bottom",
    "vertical-align", "text-align",
    "padding",
    "padding-top", "padding-right",
    "padding-bottom", "padding-left",
    "margin",
    "margin-top", "margin-right",
    "margin-bottom", "margin-left", "cursor",
    "border-top", "border-right",
    "border-bottom", "border-left",
    "border-color-top", "border-color-right",
    "border-color-bottom", "border-color-left",
    "border-width-top", "border-width-right",
    "border-width-bottom", "border-width-left",
    "color", "overflow-x", "overflow-y",
    "opacity",
    "font-family", "font-style", "font-variant",
    "font-weight", "font-size",
    "background-color", "background-image", "background-repeat",
    "background-attachment", "background-position",
    "text-decoration", "white-space",
    "table-layout", "border-spacing",
    "border-collapse",
    "page-break-before", "page-break-after",
    "zoom", "visibility", "display",
    "box-sizing", "flex", "flex-flow", "align-self", "justify-content"};

std::string cssCamelNames_[] =
  { "cssText", "width", "position",
    "zIndex", "cssFloat", "clear",
    "width", "height", "lineHeight",
    "minWidth", "minHeight",
    "maxWidth", "maxHeight",
    "left", "right", "top", "bottom",
    "verticalAlign", "textAlign",
    "padding",
    "paddingTop", "paddingRight",
    "paddingBottom", "paddingLeft",
    "margin",
    "marginTop", "marginRight",
    "marginBottom", "marginLeft",
    "cursor",
    "borderTop", "borderRight",
    "borderBottom", "borderLeft",
    "borderColorTop", "borderColorRight",
    "borderColorBottom", "borderColorLeft",
    "borderWidthTop", "borderWidthRight",
    "borderWidthBottom", "borderWidthLeft",
    "color", "overflowX", "overflowY",
    "opacity",
    "fontFamily", "fontStyle", "fontVariant",
    "fontWeight", "fontSize",
    "backgroundColor", "backgroundImage", "backgroundRepeat",
    "backgroundAttachment", "backgroundPosition",
    "textDecoration", "whiteSpace",
    "tableLayout", "borderSpacing",
    "border-collapse",
    "pageBreakBefore", "pageBreakAfter",
    "zoom", "visibility", "display",
    "boxSizing", "flex", "flexFlow", "alignSelf", "justifyContent"
  };

const std::string unsafeChars_ = " $&+,:;=?@'\"<>#%{}|\\^~[]`/";

inline char hexLookup(int n) {
  return "0123456789abcdef"[(n & 0xF)];
}

#ifndef WT_TARGET_JAVA
static_assert(sizeof(elementNames_) / sizeof(elementNames_[0]) == static_cast<unsigned int>(Wt::DomElementType::UNKNOWN), "There should be as many element names as there are dom elements (excluding unknown and other)");
static_assert(sizeof(defaultInline_) / sizeof(defaultInline_[0]) == static_cast<unsigned int>(Wt::DomElementType::UNKNOWN), "defaultInline_ should be the same size as the number of dom elements (excluding unknown and other)");
#endif // WT_TARGET_JAVA

}



template <>
struct fmt::formatter<EscapedString> {
    char jstype = 0; // Default to double quote escaping
    char htmltype = 0;
    //RuleSet jsrule, htmlrule;
    // Parse format specifiers (e.g., {d} for double quote, {s} for single quote)
    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && (*it == 'd' || *it == 's')) {
            // if(*it == 'd')
            //     jsrule = RuleSet::JsStringLiteralDQuote;
            // else if(*it == 's')
            //     jsrule = RuleSet::JsStringLiteralSQuote;
            jstype = *it++;
        }
        if(it != end && (*it == 'h' || *it == 'p' || *it == 'n')) {
            // if(*it == 'h')
            //     htmlrule = RuleSet::HtmlAttribute;
            // else if(*it == 'p')
            //     htmlrule = RuleSet::PlainText;
            // else if(*it == 'n')
            //     htmlrule = RuleSet::PlainTextNewLines;
            htmltype = *it++;
        }
        if (it != end && *it != '}') {
            throw format_error("invalid format specifier");
        }
        return it;
    }

    // Format based on specifier
    template <typename FormatContext>
    auto format(const EscapedString& jsv, FormatContext& ctx) const {
        if(jsv.isescaped_) {
            return fmt::format_to(ctx.out(), FMT_COMPILE("{}"), jsv.value);
        }

        std::string escaped;

        if(jstype && !htmltype) {
            if(jstype == 's') {
                using Escaper = MixedRules<RuleSet::JsStringLiteralSQuote>;
                Escaper::escape(jsv.value, escaped);
                return fmt::format_to(ctx.out(), FMT_COMPILE("{}"), escaped);
            }
            else {
                using Escaper = MixedRules<RuleSet::JsStringLiteralDQuote>;
                Escaper::escape(jsv.value, escaped);
                return fmt::format_to(ctx.out(), FMT_COMPILE("{}"), escaped);
            }
        }
        else if(htmltype && !jstype) {
            if(htmltype == 'h') {
                using Escaper = MixedRules<RuleSet::HtmlAttribute>;
                Escaper::escape(jsv.value, escaped);
                return fmt::format_to(ctx.out(), FMT_COMPILE("{}"), escaped);
            }
            else if(htmltype == 'p') {
                using Escaper = MixedRules<RuleSet::PlainText>;
                Escaper::escape(jsv.value, escaped);
                return fmt::format_to(ctx.out(), FMT_COMPILE("{}"), escaped);
            }
            else {
                using Escaper = MixedRules<RuleSet::PlainTextNewLines>;
                Escaper::escape(jsv.value, escaped);
                return fmt::format_to(ctx.out(), FMT_COMPILE("{}"), escaped);
            }
        }
        else if(htmltype && jstype) {
            if(htmltype == 'h') {
                if(jstype == 's') {
                    using Escaper = MixedRules<RuleSet::HtmlAttribute, RuleSet::JsStringLiteralSQuote>;
                    Escaper::escape(jsv.value, escaped);
                    return fmt::format_to(ctx.out(), FMT_COMPILE("{}"), escaped);
                }
                else {
                    using Escaper = MixedRules<RuleSet::HtmlAttribute, RuleSet::JsStringLiteralDQuote>;
                    Escaper::escape(jsv.value, escaped);
                    return fmt::format_to(ctx.out(), FMT_COMPILE("{}"), escaped);
                }
            }
            else if(htmltype == 'p') {
                if(jstype == 's') {
                    using Escaper = MixedRules<RuleSet::PlainText, RuleSet::JsStringLiteralSQuote>;
                    Escaper::escape(jsv.value, escaped);
                    return fmt::format_to(ctx.out(), FMT_COMPILE("{}"), escaped);
                }
                else {
                    using Escaper = MixedRules<RuleSet::PlainText, RuleSet::JsStringLiteralDQuote>;
                    Escaper::escape(jsv.value, escaped);
                    return fmt::format_to(ctx.out(), FMT_COMPILE("{}"), escaped);
                }
            }
            else {
                if(jstype == 's') {
                    using Escaper = MixedRules<RuleSet::PlainTextNewLines, RuleSet::JsStringLiteralSQuote>;
                    Escaper::escape(jsv.value, escaped);
                    return fmt::format_to(ctx.out(), FMT_COMPILE("{}"), escaped);
                }
                else {
                    using Escaper = MixedRules<RuleSet::PlainTextNewLines, RuleSet::JsStringLiteralDQuote>;
                    Escaper::escape(jsv.value, escaped);
                    return fmt::format_to(ctx.out(), FMT_COMPILE("{}"), escaped);
                }
            }
        }
        //if no parameter
        return fmt::format_to(ctx.out(), FMT_COMPILE("{}"), jsv.value);
    }
};

template <>
class fmt::formatter<std::pair<const Wt::Property, std::string>> {
    // format specification storage
    char presentation_ = 's';
    bool onlystyle_ = false;
public:
    // parse format specification and store it:
    constexpr auto parse (format_parse_context& ctx) {
        auto i = ctx.begin(), end = ctx.end();
        if (i != end && (*i == 's' || *i == 'e')) {
            presentation_ = *i++;
            onlystyle_ = true;
        }
        if (i != end && *i != '}') {
            throw format_error("invalid format");
        }
        return i;
    }
    // format a value using stored specification:
    template <typename FormatContext>
    constexpr auto format(const std::pair<const Wt::Property, const std::string>& pair, FormatContext &ctx) const {
        using namespace Wt;
        using Parser = MixedRules<RuleSet::HtmlAttribute>;

        auto out = ctx.out();

        auto &[prop, value] = pair;
        unsigned p = static_cast<unsigned int>(prop);

        if(prop == Wt::Property::Style) {
            out = fmt::format_to(out, FMT_COMPILE("{:h}"), JsString(value));
        }
        else if ((p >= static_cast<unsigned int>(Wt::Property::StylePosition)) &&
                 (p < static_cast<unsigned int>(Wt::Property::LastPlusOne))) {
            out = fmt::format_to(out, FMT_COMPILE("{}:{};"), cssNames_[p - static_cast<unsigned int>(Wt::Property::StylePosition)], value);
        }
        else if (prop == Wt::Property::StyleWidthExpression) {
            out = fmt::format_to(out, FMT_COMPILE("width:expression({});"), value);
        }

        if(onlystyle_)
            return out;

        switch (prop) {
        case Property::InnerHTML:
            //innerHTML += i->second;
            break;
        case Property::Disabled:
            if (value == "true")
                //out << " disabled=\"disabled\"";
            break;
        case Property::ReadOnly:
            if (value == "true")
                out = fmt::format_to(out," readonly=\"readonly\"");
                //out << " readonly=\"readonly\"";
            break;
        case Property::TabIndex:
            out = fmt::format_to(out, FMT_COMPILE(" tabindex=\"{}\""), value);
            break;
        case Property::Checked:
            if (value == "true")
                out = fmt::format_to(out," checked=\"checked\"", value);
                //out << " checked=\"checked\"";
            break;
        case Property::Selected:
            if (value == "true")
                out = fmt::format_to(out," selected=\"selected\"", value);
                //out << " selected=\"selected\"";
            break;
        case Property::SelectedIndex:
            if (value == "-1") {
                // DomElement *self = const_cast<DomElement *>(this);
                // self->callMethod("selectedIndex=-1");
            }
            break;
        case Property::Multiple:
            if (value == "true")
                out = fmt::format_to(out," multiple=\"multiple\"", value);

                //out << " multiple=\"multiple\"";
            break;
        case Property::Target:
            out = fmt::format_to(out, FMT_COMPILE(" target=\"{}\""), value);
            break;
        case Property::Download:
            out = fmt::format_to(out, FMT_COMPILE(" download=\"{}\""), value);
            break;
        // case Property::Indeterminate:
        //     if (value == "true") {
        //         DomElement *self = const_cast<DomElement *>(this);
        //         self->callMethod("indeterminate=" + value);
        //     }
        //     break;
        // case Property::Value:
        //     if (type_ != DomElementType::TEXTAREA) {
        //         out = fmt::format_to(out," value=\"{}\"", value);
        //     } else {
        //         std::string v = value;
        //         innerHTML += WWebWidget::escapeText(v, false);
        //     }
        //     break;
        case Property::Src:
            out = fmt::format_to(out, FMT_COMPILE(" src=\"{}\""), value);
            break;
        case Property::ColSpan:
            out = fmt::format_to(out, FMT_COMPILE(" colspan=\"{}\""), value);
            break;
        case Property::RowSpan:
            out = fmt::format_to(out, FMT_COMPILE(" rowspan=\"{}\""), value);
            break;
        case Property::Class:
            out = fmt::format_to(out, FMT_COMPILE(" class=\"{}\""), value);
            break;
        case Property::Label:
            out = fmt::format_to(out, FMT_COMPILE(" label=\"{}\""), value);
            break;
        case Property::Placeholder:
            out = fmt::format_to(out, FMT_COMPILE(" placeholder=\"{}\""), value);
            break;
        default:
            break;
        }
        return out;
    }
};

template <>
struct fmt::formatter<const Wt::DomElement*> {    // format specification storage
    char presentation_ = 's';
    bool onlystyle_ = false;
public:
    // parse format specification and store it:
    constexpr auto parse (format_parse_context& ctx) {
        auto i = ctx.begin(), end = ctx.end();
        if (i != end && (*i == 's' || *i == 'a' || *i == 'p')) {
            presentation_ = *i++;
            onlystyle_ = true;
        }
        if (i != end && *i != '}') {
            throw format_error("invalid format");
        }
        return i;
    }
    // format a value using stored specification:
    template <typename FormatContext>
    auto format(const Wt::DomElement* domElement, FormatContext& ctx) const {
        using namespace Wt;

        auto out = ctx.out();

        if(presentation_ == 's') {
            auto& properties = domElement->properties_;

            if(onlystyle_ && (domElement->hasCssRules_ || domElement->isDefaultInline()))
                out = fmt::format_to(out, FMT_COMPILE(" style=\"{}{:s}\""), domElement->isDefaultInline() ? "display: block;" : "", fmt::join(properties, ""));

            if (auto i = properties.find(Property::Disabled); (i != properties.end()) && (i->second=="true"))
                out = fmt::format_to(out," disabled=\"disabled\"");

            if (auto j = domElement->attributes_.find("title"); j != domElement->attributes_.end())
            {
                out = fmt::format_to(out, FMT_COMPILE(" title=\"{:h}\""),  j->second);
            }

        } else if(presentation_ == 'a') {
            auto& attributes = domElement->attributes_;
            if (!domElement->id_.empty()) {
                out = fmt::format_to(out, FMT_COMPILE(" id=\"{}\""), domElement->id_);
            }

            for (auto i = attributes.begin(); i != attributes.end(); ++i)
                if (!WApplication::instance()->environment().agentIsSpiderBot() || i->first != "name") {
                    out = fmt::format_to(out, FMT_COMPILE(" {}=\"{:h}\""), i->first, i->second);
                }
        // }
        // else if(presentation_ == 'p') {
            using Escaper = MixedRules<RuleSet::HtmlAttribute>;
            auto& properties = domElement->properties_;
            for (auto &[prop, value] : properties) {
                switch (prop) {
                case Property::InnerHTML:
                    //innerHTML += value;
                    Escaper::escape(value, domElement->innerHTML_);
                    break;
                case Property::Disabled:
                    if (value == "true")
                        out = fmt::format_to(out," disabled=\"disabled\"");
                    break;
                case Property::ReadOnly:
                    if (value == "true")
                        out = fmt::format_to(out," readonly=\"readonly\"");
                    break;
                case Property::TabIndex:
                    out = fmt::format_to(out, FMT_COMPILE(" tabindex=\"{}\""), value);
                    break;
                case Property::Checked:
                    if (value == "true")
                        out = fmt::format_to(out," checked=\"checked\"");
                    break;
                case Property::Selected:
                    if (value == "true")
                        out = fmt::format_to(out," selected=\"selected\"");
                    break;
                case Property::SelectedIndex:
                    if (value == "-1") {
                        DomElement *self = const_cast<DomElement *>(domElement);
                        self->callMethod("selectedIndex=-1");
                    }
                    break;
                case Property::Multiple:
                    if (value == "true")
                        out = fmt::format_to(out," multiple=\"multiple\"");
                    break;
                case Property::Target:
                    out = fmt::format_to(out, FMT_COMPILE(" target=\"{}\""), value);
                    break;
                case Property::Download:
                    out = fmt::format_to(out, FMT_COMPILE(" download=\"{}\""), value);
                    break;
                case Property::Indeterminate:
                    if (value == "true") {
                        DomElement *self = const_cast<DomElement *>(domElement);
                        self->callMethod("indeterminate=" + value);
                    }
                    break;
                case Property::Value:
                    if (domElement->type_ != DomElementType::TEXTAREA) {
                        out = fmt::format_to(out, FMT_COMPILE(" value=\"{}\""), value);
                    } else {
                        std::string v = value;
                        domElement->innerHTML_ = WWebWidget::escapeText(v, false);
                        //fmt::format_to(std::back_inserter(domElement->innerHTML_), "{}\n", value);
                    }
                    break;
                case Property::Src:
                    out = fmt::format_to(out, FMT_COMPILE(" src=\"{:h}\""), JsString(value));
                    break;
                case Property::ColSpan:
                    out = fmt::format_to(out, FMT_COMPILE(" colspan=\"{:h}\""), JsString(value));
                    break;
                case Property::RowSpan:
                    out = fmt::format_to(out, FMT_COMPILE(" rowspan=\"{:h}\""), JsString(value));
                    break;
                case Property::Class:
                    out = fmt::format_to(out, FMT_COMPILE(" class=\"{:h}\""), JsString(value));
                    break;
                case Property::Label:
                    out = fmt::format_to(out, FMT_COMPILE(" label=\"{:h}\""), JsString(value));
                    break;
                case Property::Placeholder:
                    out = fmt::format_to(out, FMT_COMPILE(" placeholder=\"{:h}\""), JsString(value));
                    break;
                default:
                    break;
                }
            }
            // style
            auto app = WApplication::instance();
            if(!domElement->needButtonWrap_)
                out = fmt::format_to(out, FMT_COMPILE(" style=\"{}{:s}\""), app->environment().agent() != UserAgent::Konqueror
                                                                       && !app->environment().agentIsWebKit()
                                                                       && !app->environment().agentIsIE() ? "margin: 0px -3px -2px -3px;" : "", fmt::join(properties, ""));
            else if(app->environment().agent() != UserAgent::Konqueror
                     && !app->environment().agentIsWebKit()
                     && !app->environment().agentIsIE())
                out = fmt::format_to(out, " style=\"margin: 0px -3px -2px -3px;\"");

        }
        // else if(presentation_ == 'd') {
        //     auto app = WApplication::instance();
        //     if(!domElement->needButtonWrap_)
        //         out = fmt::format_to(out, " style=\"{}{:s}\"", app->environment().agent() != UserAgent::Konqueror
        //                                                                && !app->environment().agentIsWebKit()
        //                                                                && !app->environment().agentIsIE() ? "margin: 0px -3px -2px -3px;" : "", ctx.out(), domElement->properties_);
        //     else if(app->environment().agent() != UserAgent::Konqueror
        //              && !app->environment().agentIsWebKit()
        //              && !app->environment().agentIsIE())
        //         out = fmt::format_to(out, " style=\"margin: 0px -3px -2px -3px;\"");

        // }
        return out;

    }
};


template <>
struct fmt::formatter<std::tuple<const Wt::DomElement&, // format specification storage
                                 fmt::memory_buffer&,
                                 fmt::memory_buffer&,
                                 std::vector<Wt::DomElement::TimeoutEvent>&>>
{
public:
    // parse format specification and store it:
    constexpr auto parse (format_parse_context& ctx) {return ctx.begin(); }
    // format a value using stored specification:
    template <typename FormatContext>
    auto format(const std::tuple<const Wt::DomElement&,
                                 fmt::memory_buffer&,
                                 fmt::memory_buffer&,
                                 std::vector<Wt::DomElement::TimeoutEvent>&>& tuple, FormatContext& ctx) const
    {

         auto &[domElement, outf, js, timeouts] = tuple;
        /*
         * http://www.w3.org/TR/html/#guidelines
         * XHTML recommendation, back-wards compatibility with HTML: C.2, C.3:
         * do not use minimized forms when content is empty like <p />, and use
         * minimized forms for certain elements like <br />
         */
        if (Wt::DomElement::isSelfClosingTag(domElement.type_))
        {
            return fmt::format_to(ctx.out(), " />");
        }
        for(auto& child : domElement.childrenToAdd_) {
            child.child.asHTML(outf, js, timeouts);
        }

        auto app = Wt::WApplication::instance();

        return fmt::format_to(ctx.out(), FMT_COMPILE("{}{}{}</{}>"), domElement.innerHTML_, std::string_view(domElement.childrenHtml_),
                              domElement.type_ == Wt::DomElementType::DIV
                                      && app->environment().agent() == Wt::UserAgent::IE6
                                      && domElement.innerHTML_.empty()
                                      && domElement.childrenToAdd_.empty()
                                      && !domElement.childrenHtml_.size() ? "&nbsp;" : "", // IE6 will incorrectly set the height of empty divs
                              domElement.type_  == Wt::DomElementType::OTHER ?
                                  domElement.elementTagName_ : elementNames_[static_cast<unsigned int>(domElement.type_)]);
    }
};


template <>
struct fmt::formatter<Wt::DomElement::TimeoutEvent> {    // format specification storage

public:
    // parse format specification and store it:
    constexpr auto parse (format_parse_context& ctx) {return ctx.begin(); }

    // format a value using stored specification:
    template <typename FormatContext>
    auto format(const Wt::DomElement::TimeoutEvent& timeout, FormatContext& ctx) const {
        auto app = Wt::WApplication::instance();
        return fmt::format_to(ctx.out(), FMT_COMPILE("{}._p_.addTimerEvent('{}',{},{});\n"),
                              app->javaScriptClass(),
                              timeout.event,
                              timeout.msec,
                              timeout.repeat);

            // out << app->javaScriptClass()
            // << "._p_.addTimerEvent('" << timeout.event << "', "
            // << timeouts[i].msec << ','
            // << timeouts[i].repeat << ");\n";
    }
};

namespace Wt {


#if defined(WT_THREADED) || defined(WT_TARGET_JAVA)
  std::atomic<unsigned> DomElement::nextId_(0);
#else
  unsigned DomElement::nextId_ = 0;
#endif

DomElement DomElement::createNew(DomElementType type)
{
  return DomElement(Mode::Create, type);
}

DomElement DomElement::getForUpdate(const std::string &id, DomElementType type)
{
    if (id.empty())
        throw WException("Cannot update widget without id");

    DomElement e(Mode::Update, type);
    e.id_ = id;

    return e;
}

DomElement DomElement::updateGiven(const std::string& var, DomElementType type)
{
  DomElement e(Mode::Update, type);
  e.var_ = var;

  return e;
}

DomElement DomElement::getForUpdate(const WObject *object, DomElementType type)
{
  return getForUpdate(object->id(), type);
}

DomElement::DomElement(Mode mode, DomElementType type)
  : mode_(mode),
    wasEmpty_(mode_ == Mode::Create),
    removeAllChildren_(-1),
    minMaxSizeProperties_(false),
    unstubbed_(false),
    unwrapped_(false),
    replaced_(nullptr),
    insertBefore_(nullptr),
    type_(type),
    numManipulations_(0),
    timeOut_(-1),
    timeOutJSRepeat_(-1),
    globalUnfocused_(false)
{ }

DomElement::DomElement(Mode mode, DomElementType type, const std::span<const PropPair> prop, const attribList& attrib, const std::string& id) :
    mode_(mode),
    wasEmpty_(mode_ == Mode::Create),
    removeAllChildren_(-1),
    minMaxSizeProperties_(false),
    unstubbed_(false),
    unwrapped_(false),
    replaced_(nullptr),
    insertBefore_(nullptr),
    type_(type),
    numManipulations_(0),
    timeOut_(-1),
    timeOutJSRepeat_(-1),
    globalUnfocused_(false),
    id_(id),
    attributes_(attrib.begin(), attrib.end()),
    properties_ {prop.begin(), prop.end()}
{ if(!id.empty()) numManipulations_++; }

DomElement::~DomElement()
{
  // for (unsigned i = 0; i < childrenToAdd_.size(); ++i)
  //   delete childrenToAdd_[i].child;

  // for (unsigned i = 0; i < updatedChildren_.size(); ++i)
  //   delete updatedChildren_[i];

  //delete replaced_;
  delete insertBefore_;
}

void DomElement::setDomElementTagName(const std::string& name) {
  this->elementTagName_ = name;
}

#ifndef WT_TARGET_JAVA
#define toChar(b) char(b)
#else
unsigned char toChar(int b) {
  return (unsigned char)b;
}
#endif

std::string DomElement::urlEncodeS(const std::string& url,
                                   const std::string &allowed)
{
    WStringStream result;

#ifdef WT_TARGET_JAVA
    std::vector<unsigned char> bytes;
    try {
        bytes = url.getBytes("UTF-8");
    } catch (UnsupportedEncodingException& e) {
        // eat silly UnsupportedEncodingException
    }
#else
    const std::string& bytes = url;
#endif

    for (unsigned i = 0; i < bytes.size(); ++i) {
        unsigned char c = toChar(bytes[i]);
        if (c <= 31 || c >= 127 || unsafeChars_.find(c) != std::string::npos) {
            if (allowed.find(c) != std::string::npos) {
                result << (char)c;
            } else {
                result << '%';
                result << hexLookup(c >> 4);
                result << hexLookup(c);
            }
        } else
            result << (char)c;
    }

    return result.str();
}

std::string DomElement::urlEncodeS(std::string_view url, const uint8_t charset[])
{
  return ada::unicode::percent_encode(url, charset);//urlEncodeS(url, std::string());
}

// std::string DomElement::urlEncodeS(std::string_view url,
//                                    const std::string &allowed)
// {
//   WStringStream result;

// #ifdef WT_TARGET_JAVA
//   std::vector<unsigned char> bytes;
//   try {
//     bytes = url.getBytes("UTF-8");
//   } catch (UnsupportedEncodingException& e) {
//     // eat silly UnsupportedEncodingException
//   }
// #else
//   //const std::string& bytes = url;
// #endif

//   for (unsigned i = 0; i < url.size(); ++i) {
//     unsigned char c = toChar(url[i]);
//     if (c <= 31 || c >= 127 || unsafeChars_.find(c) != std::string::npos) {
//       if (allowed.find(c) != std::string::npos) {
//         result << (char)c;
//       } else {
//         result << '%';
//         result << hexLookup(c >> 4);
//         result << hexLookup(c);
//       }
//     } else
//       result << (char)c;
//   }

//   return result.str();
// }

void DomElement::setType(DomElementType type)
{
  type_ = type;
}

void DomElement::setWasEmpty(bool how)
{
  wasEmpty_ = how;
}

void DomElement::updateInnerHtmlOnly()
{
  mode_ = Mode::Update;

  assert(replaced_ == nullptr);
  assert(insertBefore_ == nullptr);

  attributes_.clear();
  removedAttributes_.clear();
  eventHandlers_.clear();

  for (PropertyMap::iterator i = properties_.begin(); i != properties_.end();) {
    if (   i->first == Property::InnerHTML || i->first == Property::Target)
      ++i;
    else
      properties_.erase(i++);
  }
}

void DomElement::addChild(DomElement &&child)
{
  if (child.mode() == Mode::Create) {
    numManipulations_ += 2; // cannot be short-cutted

    if (wasEmpty_ && canWriteInnerHTML(WApplication::instance())) {
      child.asHTML(childrenHtml_, javaScript_, timeouts_);
      //delete child;
    } else {
      childrenToAdd_.push_back(ChildInsertion(-1, std::move(child)));
    }
  } else
    updatedChildren_.push_back(std::move(child));
}

void DomElement::saveChild(const std::string& id)
{
  childrenToSave_.push_back(id);
}



std::string DomElement::getAttribute(const std::string& attribute) const
{

    if (auto i = attributes_.find(attribute); i != attributes_.end())
        return i->second.value;

    return std::string();
}

void DomElement::removeAttribute(const std::string& attribute)
{
  ++numManipulations_;
  attributes_.erase(attribute);
  removedAttributes_.insert(attribute);
}

void DomElement::setEventSignal(const char *eventName,
				const EventSignalBase& signal)
{
  setEvent(eventName, signal.javaScript(), signal.encodeCmd(), signal.isExposedSignal());
}

void DomElement::setEvent(const char *eventName,
                          const std::string& jsCode,
                          const std::string& signalName,
                          bool isExposed)
{
  WApplication *app = WApplication::instance();

    bool anchorClick = (type() == DomElementType::A) &&
                       (eventName == WInteractWidget::CLICK_SIGNAL);

  // WStringStream js;
  // if (isExposed || anchorClick || !jsCode.empty()) {

  //   js << "var e=event||window.event,";
  //   js << "o=this;";

  //   if (anchorClick)
  //     js << "if(e.ctrlKey||e.metaKey||e.shiftKey||(" WT_CLASS ".button(e) > 1))return true;else{";

  //   /*
  //    * This order, first JavaScript and then event propagation is important
  //    * for WCheckBox where the tristate state is cleared before propagating
  //    * its value
  //    */
  //   js << jsCode;

  //   if (isExposed)
  //     js << app->javaScriptClass() << "._p_.update(o,'"
     // << signalName << "',e,true);";

  //   if (anchorClick)
  //     js << "}";
  // }

   ++numManipulations_;
  // eventHandlers_[eventName] = EventHandler(js.str(), signalName);

  if(isExposed && anchorClick)
      eventHandlers_[eventName] = EventHandler(fmt::format(FMT_COMPILE("var e=event||window.event,o=this;if(e.ctrlKey||e.metaKey||e.shiftKey||(" WT_CLASS ".button(e) > 1))return true;else{{{}{}._p_.update(o,'{}',e,true);}}"),
                                                           jsCode,
                                                           app->javaScriptClass(),
                                                           signalName), signalName);
  else if(isExposed)
      eventHandlers_[eventName] = EventHandler(fmt::format(FMT_COMPILE("var e=event||window.event,o=this;{}._p_.update(o,'{}',e,true);"),
                                                           app->javaScriptClass(),
                                                           signalName), signalName);
  else if(anchorClick)
      eventHandlers_[eventName] = EventHandler(fmt::format(FMT_COMPILE("var e=event||window.event,o=this;if(e.ctrlKey||e.metaKey||e.shiftKey||(" WT_CLASS ".button(e) > 1))return true;else{{{}}}"), jsCode), signalName);
  else
      eventHandlers_[eventName] = EventHandler(fmt::format(FMT_COMPILE("var e=event||window.event,o=this;{}"), jsCode), signalName);

}

void DomElement::setEvent(const char *eventName, const std::string& jsCode)
{
  eventHandlers_[eventName] = EventHandler(jsCode, std::string());
}

void DomElement::addEvent(const char *eventName, const std::string& jsCode)
{
  eventHandlers_[eventName].jsCode += jsCode;
}

DomElement::EventAction::EventAction(const std::string& aJsCondition,
				     const std::string& aJsCode,
				     const std::string& anUpdateCmd,
				     bool anExposed)
  : jsCondition(aJsCondition),
    jsCode(aJsCode),
    updateCmd(anUpdateCmd),
    exposed(anExposed)
{ }

void DomElement::setEvent(const char *eventName,
			  const std::vector<EventAction>& actions)
{
  WStringStream code;

  for (unsigned i = 0; i < actions.size(); ++i) {
    if (!actions[i].jsCondition.empty())
      code << "if(" << actions[i].jsCondition << "){";

    /*
     * This order, first JavaScript and then event propagation is important
     * for WCheckBox where the tristate state is cleared before propagating
     * its value
     */
    code << actions[i].jsCode;

    if (actions[i].exposed)
      code << WApplication::instance()->javaScriptClass()
	   << "._p_.update(o,'" << actions[i].updateCmd << "',e,true);";

    if (!actions[i].jsCondition.empty())
      code << "}";
  }

  // for(auto& action : actions) {
  //   if (!action.jsCondition.empty() && action.exposed)
  //       setEvent(eventName, fmt::format("if({}){{{}{}._p_.update(o,'{}',e,true);}}", action.jsCondition, action.jsCode, WApplication::instance()->javaScriptClass(), action.updateCmd), "");
  //   else if(!action.jsCondition.empty())
  //       setEvent(eventName, fmt::format("if({}){{{}}}", action.jsCondition, action.jsCode), "");
  //   else if(action.exposed)
  //       setEvent(eventName, fmt::format("{}{}._p_.update(o,'{}',e,true);", action.jsCode, WApplication::instance()->javaScriptClass(), action.updateCmd), "");
  //   else
  //       setEvent(eventName, action.jsCode, "");
  // }

  setEvent(eventName, code.str(), "");
}

void DomElement::processProperties(WApplication *app) const
{
    if (minMaxSizeProperties_ && app->environment().agent() == UserAgent::IE6) //IE6 is really deprecated in 2025
    {
        DomElement *self = const_cast<DomElement *>(this);


        auto minw_it = self->properties_.find(Property::StyleMinWidth);
        auto maxw_it = self->properties_.find(Property::StyleMaxWidth);

        if (minw_it != properties_.end() || maxw_it != properties_.end()) {
            if (auto w = self->properties_.at(Property::StyleWidth); !w.empty()) {
                // WStringStream expr;
                // expr << WT_CLASS ".IEwidth(this,";
                // if (!minw.empty()) {
                //     expr << '\'' << minw.mapped() << '\'';
                //     self->properties_.erase(Property::StyleMinWidth); // C++: could be minw
                // } else
                //     expr << "'0px'";
                // expr << ',';
                // if (!maxw.empty()) {
                //     expr << '\''<< maxw.mapped() << '\'';
                //     self->properties_.erase(Property::StyleMaxWidth); // C++: could be maxw
                // } else
                //     expr << "'100000px'";
                // expr << ")";
                auto minw = minw_it != properties_.end() ? minw_it->second : "0px";
                auto maxw = maxw_it != properties_.end() ? maxw_it->second : "100000px";

                //fmt::memory_buffer buf;


                // self->properties_.erase(Property::StyleMinWidth);
                // self->properties_.erase(Property::StyleMaxWidth);

                self->properties_.erase(Property::StyleWidth);
                self->properties_[Property::StyleWidthExpression] = fmt::format(FMT_COMPILE(WT_CLASS ".IEwidth(this,\'{}\',\'{}\')"),
                                                                                minw,
                                                                                maxw);
            }
        }



        if (auto i = self->properties_.find(Property::StyleMinHeight); i != self->properties_.end()) {
            self->properties_[Property::StyleHeight] = i->second;
        }
    }
}

void DomElement::processEvents(WApplication *app) const
{
  DomElement *self = const_cast<DomElement *>(this);

  const char *S_keypress = WInteractWidget::KEYPRESS_SIGNAL;

  if (auto keypress = self->eventHandlers_.find(S_keypress); keypress != eventHandlers_.end() && !keypress->second.jsCode.empty())
    keypress->second.jsCode = fmt::format("if (" WT_CLASS ".isKeyPress(event)){{{}}}", keypress->second.jsCode);
}

void DomElement::setTimeout(int msec, bool jsRepeat)
{
  ++numManipulations_;
  timeOut_ = msec;
  timeOutJSRepeat_ = jsRepeat ? msec : -1;
}

void DomElement::setTimeout(int delay, int interval)
{
  ++numManipulations_;
  timeOut_ = delay;
  timeOutJSRepeat_ = interval;
}

void DomElement::callJavaScript(const std::string& jsCode, bool evenWhenDeleted)
{
  ++numManipulations_;
  if (!evenWhenDeleted)
    fmt::format_to(std::back_inserter(javaScript_), "{}\n", jsCode);
  else
    javaScriptEvenWhenDeleted_ += jsCode;
}

void DomElement::setProperties(const PropertyMap& properties)
{
  for (PropertyMap::const_iterator i = properties.begin();
       i != properties.end(); ++i)
    setProperty(i->first, i->second);
}

void DomElement::clearProperties()
{
  numManipulations_ -= properties_.size();
  properties_.clear();
}



void DomElement::addPropertyWord(Property property, const std::string& value)
{
  PropertyMap::const_iterator i = properties_.find(property);
  
  if (i != properties_.end()) {
    Utils::SplitSet words;
    Utils::split(words, i->second, " ", true);
    if (words.find(value) != words.end())
      return;
  }

  setProperty(property, Utils::addWord(getProperty(property), value));
}

std::string DomElement::getProperty(Property property) const
{
    if (auto i = properties_.find(property); i != properties_.end())
        return i->second;

    return std::string();
}

void DomElement::removeProperty(Property property)
{
  properties_.erase(property);
}

void DomElement::setId(const std::string& id)
{
  ++numManipulations_;
  id_ = id;
}

void DomElement::setName(const std::string& name)
{
  ++numManipulations_;
  id_ = name;
  setAttribute("name", name);
}

void DomElement::insertChildAt(DomElement &child, int pos)
{
  ++numManipulations_;

  childrenToAdd_.push_back(ChildInsertion(pos, std::move(child)));
}

void DomElement::insertBefore(DomElement *sibling)
{
  ++numManipulations_;
  insertBefore_ = sibling;
}

void DomElement::removeFromParent()
{
  callJavaScript(WT_CLASS ".remove('" + id() + "');", true);
}

void DomElement::replaceWith(DomElement &&newElement)
{
    ++numManipulations_;
    replaced_ = std::make_unique<DomElement>(std::forward<DomElement>(newElement));
}

// void DomElement::replaceWith(std::unique_ptr<DomElement> newElement)
// {
//     ++numManipulations_;
//     replaced_ = std::move(newElement);
// }

void DomElement::removeAllChildren(int firstChild)
{
  ++numManipulations_;
  removeAllChildren_ = firstChild;
  wasEmpty_ = firstChild == 0;
}

void DomElement::unstubWith(DomElement&& newElement, bool hideWithDisplay)
{
  replaceWith(std::forward<DomElement>(newElement));
  unstubbed_ = true;
  hideWithDisplay_ = hideWithDisplay;
}

void DomElement::unwrap()
{
  ++numManipulations_;
  unwrapped_ = true;
}

// void DomElement::callMethod(const std::string& method)
// {
//   ++numManipulations_;

//   if (var_.empty())
//     javaScript_ << WT_CLASS << ".$('" << id_ << "').";
//   else
//     javaScript_ << var_ << '.';

//   javaScript_ << method << ";\n";
// }

void DomElement::callMethod(std::string_view method)
{
    ++numManipulations_;

    if (var_.empty())
        fmt::format_to(std::back_inserter(javaScript_), FMT_COMPILE("{}.$('{}').{};\n"), WT_CLASS, id_, method);
    else
        fmt::format_to(std::back_inserter(javaScript_), FMT_COMPILE("{}.{};\n"), var_, method);
}

void DomElement::jsStringLiteral(WStringStream& out, const std::string& s,
				 char delimiter)
{
  EscapeOStream sout(out);
  jsStringLiteral(sout, s, delimiter);
}

void DomElement::htmlAttributeValue(WStringStream& out, const std::string& s)
{
  EscapeOStream sout(out);
  sout.pushEscape(EscapeOStream::HtmlAttribute);
  sout << s;
}

void DomElement::fastJsStringLiteral(EscapeOStream& outRaw,
				     const EscapeOStream& outEscaped,
				     const std::string& s)
{
  outRaw << '\'';
  outRaw.append(s, outEscaped);
  outRaw << '\'';
}

void DomElement::jsStringLiteral(EscapeOStream& out, const std::string& s,
				 char delimiter)
{
  out << delimiter;

  out.pushEscape(delimiter == '\'' ?
		 EscapeOStream::JsStringLiteralSQuote :
		 EscapeOStream::JsStringLiteralDQuote);
  out << s;
  out.popEscape();

  out << delimiter;
}

void DomElement::fastHtmlAttributeValue(EscapeOStream& outRaw,
					const EscapeOStream& outEscaped,
					const std::string& s)
{
  outRaw << '"';
  outRaw.append(s, outEscaped);
  outRaw << '"';
}

std::string DomElement::cssStyle() const
{
  if (properties_.empty())
    return std::string();

  EscapeOStream style;
  const std::string *styleProperty = nullptr;

  for (PropertyMap::const_iterator j = properties_.begin();
       j != properties_.end(); ++j) {
    unsigned p = static_cast<unsigned int>(j->first);

    if (j->first == Property::Style)
      styleProperty = &j->second;
    else if ((p >= static_cast<unsigned int>(Property::StylePosition)) &&
             (p < static_cast<unsigned int>(Property::LastPlusOne))) {
      if (!j->second.empty()) {
    style << cssNames_[p -
                       static_cast<unsigned int>(Property::StylePosition)]
          << ':' << j->second << ';';
    if (p >= static_cast<unsigned int>(Property::StyleBoxSizing)) {
      WApplication *app = WApplication::instance();

      if (app) {
          if (app->environment().agentIsGecko())
              style << "-moz-";
          else if (app->environment().agentIsWebKit())
              style << "-webkit-";
      }

      style << cssNames_[p -
                         static_cast<unsigned int>(Property::StylePosition)]
            << ':' << j->second << ';';
    }
      }
    } else if (j->first == Property::StyleWidthExpression) {
      style << "width:expression(" << j->second << ");";
    }
  }

  if (styleProperty)
    style << *styleProperty;

  return style.c_str();
}

void DomElement::cssStyle(fmt::memory_buffer &out) const
{
    if (properties_.empty() || !hasCssRules_)
        return;

    fmt::format_to(std::back_inserter(out), FMT_COMPILE(" style=\"{}\""), fmt::join(properties_, ""));


    // for(auto&[prop, value] : std::views::reverse(properties_)) {
    //     unsigned p = static_cast<unsigned int>(prop);

    //     if(prop == Property::Style) {
    //         fmt::format_to(std::back_inserter(out), "{}", value);
    //     }
    //     else if ((p >= static_cast<unsigned int>(Property::StylePosition)) &&
    //              (p < static_cast<unsigned int>(Property::LastPlusOne))) {
    //         fmt::format_to(std::back_inserter(out), "{}:{};", cssNames_[p - static_cast<unsigned int>(Property::StylePosition)], value);
    //     }
    //     else if (prop == Property::StyleWidthExpression) {
    //         fmt::format_to(std::back_inserter(out), "width:expression({});", value);
    //     }
    // }
}

void DomElement::setJavaScriptEvent(EscapeOStream& out,
				    const char *eventName,
				    const EventHandler& handler,
				    WApplication *app) const
{
  // events on the dom root container are events received by the whole
  // document when no element has focus

  unsigned fid = nextId_++;

  out << "function f" << fid << "(event) { ";

  out << handler.jsCode;

  out << "}\n";

  if (globalUnfocused_) {
    out << app->javaScriptClass() 
      <<  "._p_.bindGlobal('" << std::string(eventName) <<"', '" << id_ << "', f" << fid 
      << ")\n";
    return;
  } else {
    declare(out);
    out << var_;
  }

  if (eventName == WInteractWidget::WHEEL_SIGNAL &&
      app->environment().agentIsIE() && 
      static_cast<unsigned int>(app->environment().agent()) >= 
      static_cast<unsigned int>(UserAgent::IE9))
    out << ".addEventListener('wheel', f" << fid << ", false);\n";
  else //mode_ == update
    out << ".on" << const_cast<char *>(eventName) << "=f" << fid << ";\n";
}
void DomElement::setJavaScriptEvent(fmt::memory_buffer& out,
                                    const char *eventName,
                                    const EventHandler& handler,
                                    WApplication *app) const
{
    // events on the dom root container are events received by the whole
    // document when no element has focus

    unsigned fid = nextId_++;

    if (globalUnfocused_ ||
        (eventName == WInteractWidget::WHEEL_SIGNAL &&
         app->environment().agentIsIE() &&
         static_cast<unsigned int>(app->environment().agent()) >= static_cast<unsigned int>(UserAgent::IE9)))
    {
        if (globalUnfocused_) {
            fmt::format_to(std::back_inserter(out), FMT_COMPILE("function f{fid}(event){{{}}}\n{}._p_.bindGlobal('{}', '{}', f{fid})\n"), handler.jsCode, app->javaScriptClass(), fmt::ptr(signal), id_, fmt::arg("fid", fid));
            //return;
        } else {
            declare(out);
            fmt::format_to(std::back_inserter(out), FMT_COMPILE("function f{fid}(event){{{}}}\n{}.addEventListener('wheel', f{fid}, false);\n"), handler.jsCode, var_, fmt::arg("fid", fid)); //out << ".addEventListener('wheel', f" << fid << ", false);\n";
        }
    }
    else
    {
        fmt::format_to(std::back_inserter(out), FMT_COMPILE("function f{fid}(event){{{}}}\n{}.on{}=f{fid}"), fid, handler.jsCode, var_, eventName, fmt::arg("fid", fid));
    }
}

void DomElement::asHTML(fmt::memory_buffer &out, fmt::memory_buffer &javaScript, TimeoutList &timeouts, bool openingTagOnly) const
{
    if (mode_ != Mode::Create)
        throw WException("DomElement::asHTML() called with ModeUpdate");

    WApplication *app = WApplication::instance();
    processEvents(app);
    processProperties(app);

    auto clickEvent = eventHandlers_.find(WInteractWidget::CLICK_SIGNAL);

    needButtonWrap_
        = (!app->environment().ajax()
           && (clickEvent != eventHandlers_.end())
           && (!clickEvent->second.signalName.empty())
           && (!app->environment().agentIsSpiderBot()));

    //bool isSubmit = needButtonWrap_;
    DomElementType renderedType = type_;

    if (needButtonWrap_) {
        if (type_ == DomElementType::BUTTON) {
            /*
           * We don't need to wrap a button: we can just modify the attributes
           * type name and value. This avoid layout problems.
           *
           * Note that IE posts the button text instead of the value. We fix
           * this by encoding the value into the name.
           *
           * IE6 hell: IE will post all submit buttons, not just the one clicked.
           * We should therefore really be using input
           */
            DomElement *self = const_cast<DomElement *>(this);
            self->setAttribute("type", "submit", true);
            self->setAttribute("name", "signal=" + clickEvent->second.signalName);

            needButtonWrap_ = false;
        } else if (type_ == DomElementType::IMG) {
            /*
           * We don't need to wrap an image: we can substitute it for an input
           * type image. This avoid layout problems.
           */
            renderedType = DomElementType::INPUT;

            DomElement *self = const_cast<DomElement *>(this);
            self->setAttribute("type", "image", true);
            self->setAttribute("name", "signal=" + clickEvent->second.signalName);
            needButtonWrap_ = false;
        }
    }

    /*
   * We also should not wrap anchors, map area elements and form elements.
   */
    if (needButtonWrap_) {
        if (   type_ == DomElementType::AREA
            || type_ == DomElementType::INPUT
            || type_ == DomElementType::SELECT)
            needButtonWrap_ = false;

        if (type_ == DomElementType::A) {
            std::string href = getAttribute("href");

            /*
             * If we're IE7/8 or there is a real URL, then we don't wrap
             */
            if (app->environment().agent() == UserAgent::IE7 ||
                app->environment().agent() == UserAgent::IE8 ||
                href.length() > 1)
                needButtonWrap_ = false;
            else if (app->theme()->canStyleAnchorAsButton()) {
                DomElement *self = const_cast<DomElement *>(this);
                self->setAttribute("href", app->url(app->internalPath())
                                               + "&signal=" + clickEvent->second.signalName);
                needButtonWrap_ = false;
            }
        } else if (type_ == DomElementType::AREA) {
            DomElement *self = const_cast<DomElement *>(this);
            self->setAttribute("href", app->url(app->internalPath())
                                           + "&signal=" + clickEvent->second.signalName);
        }
    }

    //const bool supportButton = true;

    //bool needAnchorWrap = false;

    // if (!supportButton && type_ == DomElementType::BUTTON) {
    //     renderedType = DomElementType::INPUT;

    //     DomElement *self = const_cast<DomElement *>(this);
    //     if (!isSubmit)
    //         self->setAttribute("type", "button");
    //     self->setAttribute("value",
    //                        properties_.find(Property::InnerHTML)->second);
    //     self->setProperty(Property::InnerHTML, "");
    // }

    // EscapeOStream attributeValues(out);
    // attributeValues.pushEscape(EscapeOStream::HtmlAttribute);

    if (app->environment().ajax()) {
        for(auto &[signal, handler] : eventHandlers_) {
            if (!handler.jsCode.empty()) {
                if (globalUnfocused_ ||
                    (signal == WInteractWidget::WHEEL_SIGNAL &&
                     app->environment().agentIsIE() &&
                     static_cast<unsigned int>(app->environment().agent()) >= static_cast<unsigned int>(UserAgent::IE9)))
                {
                    //setJavaScriptEvent(javaScript, signal, handler, app);
                    unsigned fid = nextId_++;

                    if (globalUnfocused_) {
                        fmt::format_to(std::back_inserter(javaScript), FMT_COMPILE("function f{fid}(event){{{}}}\n{}._p_.bindGlobal('{}', '{}', f{fid})\n"), handler.jsCode, app->javaScriptClass(), signal, id_, fmt::arg("fid", fid));
                        //return;
                    } else {
                        declare(javaScript);
                        fmt::format_to(std::back_inserter(javaScript), FMT_COMPILE("function f{fid}(event){{{}}}\n{}.addEventListener('wheel', f{fid}, false);\n"), handler.jsCode, var_, fmt::arg("fid", fid)); //out << ".addEventListener('wheel', f" << fid << ", false);\n";
                    }
                }
                else
                {
                    fmt::format_to(std::back_inserter(out), FMT_COMPILE(" on{}=\"{}\""), signal, handler.jsCode);
                }
            }
        }
    }

    //std::string_view style;
    innerHTML_.clear();
    auto tuple = std::tie(*this, out, javaScript, timeouts);

    if (needButtonWrap_) {
        //if (supportButton) {
            PropertyMap& map = const_cast<PropertyMap&>(properties_);
            //auto node = map.extract(Property::Class);
            auto node = properties_.find(Property::Class);
            fmt::format_to(std::back_inserter(out), FMT_STRING("<button type=\"submit\" name=\"signal={:h}\" class=\"Wt-wrap {}\"{:s} ><{}{:a}{}{}</button>"),
                           JsString(clickEvent->second.signalName),
                           node != properties_.end() ? node->second : "",
                           this,
                           elementNames_[static_cast<unsigned int>(renderedType)],
                           this,
                           openingTagOnly || !isSelfClosingTag(renderedType) ? ">" : "",
                           tuple);

            if (node != properties_.end()) {
                map.erase(Property::Class);
            }


            // if (!isDefaultInline())
            //     fmt::format_to(std::back_inserter(out), " style=\"display: block;{}\"", fmt::join(properties_, ""));
            // else if(hasCssRules_)
            //     fmt::format_to(std::back_inserter(out), " style=\"{}\"", fmt::join(properties_, ""));
            // else {
            //     auto i = properties_.find(Property::Disabled);
            //     auto j = attributes_.find("title");
            //     fmt::format_to(std::back_inserter(out), "<button type=\"submit\" name=\"signal={}\" class=\"Wt-wrap {}\"{} title=\"{}\"><{}",
            //                    clickEvent->second.signalName,
            //                    l != properties_.end() ? l->second : "",
            //                    ((i != properties_.end()) && (i->second=="true")) ? " disabled=\"disabled\"" : "",
            //                    j != attributes_.end() ? j->second : "",
            //                    elementNames_[static_cast<unsigned int>(renderedType)]);
            // }




            // if (auto i = properties_.find(Property::Disabled); (i != properties_.end()) && (i->second=="true"))
            //     fmt::format_to(std::back_inserter(out), " disabled=\"disabled\"");

            // if (auto j = attributes_.find("title"); j != attributes_.end())
            // {
            //     fmt::format_to(std::back_inserter(out), " {}=\"{}\"", j->first, j->second);
            // }

            // if (app->environment().agent() != UserAgent::Konqueror
            //     && !app->environment().agentIsWebKit()
            //     && !app->environment().agentIsIE())
            //     style = "margin: 0px -3px -2px -3px;";

            //fmt::format_to(std::back_inserter(out), " ><{}", elementNames_[static_cast<unsigned int>(renderedType)]);
            /***************/
        // } else {
        //     auto i = properties_.find(Property::InnerHTML);
        //     if (type_ == DomElementType::IMG)
        //         fmt::format_to(std::back_inserter(out), "<input type=\"image\" name=\"signal={}\"  value=\"{}\"",
        //                        clickEvent->second.signalName,
        //                        i != properties_.end() ? i->second : "");
        //     else
        //         fmt::format_to(std::back_inserter(out), "<input type=\"submit\" name=\"signal={}\"  value=\"{}\"",
        //                        clickEvent->second.signalName,
        //                        i != properties_.end() ? i->second : "");
        // }
    // } else if (needAnchorWrap) { //DEPRECATED : never reach
    //     fmt::format_to(std::back_inserter(out), "<a href=\"#\" class=\"Wt-wrap\" onclick=\"{}\"><{}{:a}{:p}{}{}</a>",
    //                    clickEvent->second.jsCode,
    //                    elementNames_[static_cast<unsigned int>(renderedType)],
    //                    this,
    //                    this,
    //                    openingTagOnly || !isSelfClosingTag(renderedType) ? ">" : "",
    //                    ccc);
    } else /*if (renderedType == DomElementType::OTHER)*/  // Custom DomElementType
        fmt::format_to(std::back_inserter(out), FMT_STRING("<{}{:a}{}{}"),
                       renderedType == DomElementType::OTHER ? elementTagName_ : elementNames_[static_cast<unsigned int>(renderedType)],
                       this,
                       openingTagOnly || !isSelfClosingTag(renderedType) ? ">" : "",
                       tuple);
    // else
    //     fmt::format_to(std::back_inserter(out), "<{}", elementNames_[static_cast<unsigned int>(renderedType)]);


    //fmt::format_to(std::back_inserter(out), "{:a}", this);

    // if (!id_.empty()) {
    //     fmt::format_to(std::back_inserter(out), " id=\"{}\"", id_);
    // }

    // for (auto i = attributes_.begin(); i != attributes_.end(); ++i)
    //     if (!app->environment().agentIsSpiderBot() || i->first != "name") {
    //         fmt::format_to(std::back_inserter(out), " {}=\"{}\"", i->first, i->second);
    //     }

    // if (app->environment().ajax()) {
    //     for(auto &[signal, handler] : eventHandlers_) {
    //         if (!handler.jsCode.empty()) {
    //             if (globalUnfocused_ ||
    //                 (signal == WInteractWidget::WHEEL_SIGNAL &&
    //                  app->environment().agentIsIE() &&
    //                  static_cast<unsigned int>(app->environment().agent()) >= static_cast<unsigned int>(UserAgent::IE9)))
    //             {
    //                 //setJavaScriptEvent(javaScript, signal, handler, app);
    //                 unsigned fid = nextId_++;

    //                 fmt::format_to(std::back_inserter(javaScript), "function f{}(event){{{}}}\n", fid, handler.jsCode);

    //                 if (globalUnfocused_) {
    //                     fmt::format_to(std::back_inserter(javaScript), "{}._p_.bindGlobal('{}', '{}', f{})\n", app->javaScriptClass(), signal, id_, fid);
    //                     return;
    //                 } else {
    //                     declare(javaScript);
    //                 }

    //                 if (signal == WInteractWidget::WHEEL_SIGNAL &&
    //                     app->environment().agentIsIE() &&
    //                     static_cast<unsigned int>(app->environment().agent()) >= static_cast<unsigned int>(UserAgent::IE9))
    //                     fmt::format_to(std::back_inserter(javaScript), "{}.addEventListener('wheel', f{}, false);\n", var_, fid); //out << ".addEventListener('wheel', f" << fid << ", false);\n";
    //                 else
    //                     fmt::format_to(std::back_inserter(javaScript), "{}.on{}=f{};\n", var_, signal, fid); //out << ".on" << const_cast<char *>(eventName) << "=f" << fid << ";\n";
    //             }
    //             else
    //             {
    //                 fmt::format_to(std::back_inserter(out), " on{}=\"{}\"", signal, handler.jsCode);
    //             }
    //         }
    //     }
    // }




    //innerHTML_.clear();
    //fmt::format_to(std::back_inserter(out), "{:p}", this);
    // std::string innerHTML = "";

    // for (auto &[prop, value] : properties_) {
    //     switch (prop) {
    //     case Property::InnerHTML:
    //         innerHTML += value;
    //         break;
    //     case Property::Disabled:
    //         if (value == "true")
    //             fmt::format_to(std::back_inserter(out)," disabled=\"disabled\"");
    //         break;
    //     case Property::ReadOnly:
    //         if (value == "true")
    //             fmt::format_to(std::back_inserter(out)," readonly=\"readonly\"");
    //         break;
    //     case Property::TabIndex:
    //         fmt::format_to(std::back_inserter(out)," tabindex=\"{}\"", value);
    //         break;
    //     case Property::Checked:
    //         if (value == "true")
    //             fmt::format_to(std::back_inserter(out)," checked=\"checked\"");
    //         break;
    //     case Property::Selected:
    //         if (value == "true")
    //             fmt::format_to(std::back_inserter(out)," selected=\"selected\"");
    //         break;
    //     case Property::SelectedIndex:
    //         if (value == "-1") {
    //             DomElement *self = const_cast<DomElement *>(this);
    //             self->callMethod("selectedIndex=-1");
    //         }
    //         break;
    //     case Property::Multiple:
    //         if (value == "true")
    //             fmt::format_to(std::back_inserter(out)," multiple=\"multiple\"");
    //         break;
    //     case Property::Target:
    //         fmt::format_to(std::back_inserter(out)," target=\"{}\"", value);
    //         break;
    //     case Property::Download:
    //         fmt::format_to(std::back_inserter(out)," download=\"{}\"", value);
    //         break;
    //     case Property::Indeterminate:
    //         if (value == "true") {
    //             DomElement *self = const_cast<DomElement *>(this);
    //             self->callMethod("indeterminate=" + value);
    //         }
    //         break;
    //     case Property::Value:
    //         if (type_ != DomElementType::TEXTAREA) {
    //             fmt::format_to(std::back_inserter(out)," value=\"{}\"", value);
    //         } else {
    //             std::string v = value;
    //             innerHTML += WWebWidget::escapeText(v, false);
    //         }
    //         break;
    //     case Property::Src:
    //         fmt::format_to(std::back_inserter(out)," src=\"{}\"", value);
    //         break;
    //     case Property::ColSpan:
    //         fmt::format_to(std::back_inserter(out)," colspan=\"{}\"", value);
    //         break;
    //     case Property::RowSpan:
    //         fmt::format_to(std::back_inserter(out)," rowspan=\"{}\"", value);
    //         break;
    //     case Property::Class:
    //         fmt::format_to(std::back_inserter(out)," class=\"{}\"", value);
    //         break;
    //     case Property::Label:
    //         fmt::format_to(std::back_inserter(out)," label=\"{}\"", value);
    //         break;
    //     case Property::Placeholder:
    //         fmt::format_to(std::back_inserter(out)," placeholder=\"{}\"", value);
    //         break;
    //     default:
    //         break;
    //     }
    // }

    // if (!needButtonWrap_) {
    //     cssStyle(out);
    // }
    // else if(!style.empty())
    //     fmt::format_to(std::back_inserter(out), "style=\"{}\"", style);

    // if (needButtonWrap_ && !supportButton)
    //     out.append(" />");
    // else {
    // if (openingTagOnly) {
    //     out.push_back('>');
    //     return;
    // }

        /*
     * http://www.w3.org/TR/html/#guidelines
     * XHTML recommendation, back-wards compatibility with HTML: C.2, C.3:
     * do not use minimized forms when content is empty like <p />, and use
     * minimized forms for certain elements like <br />
     */
     //   if (!isSelfClosingTag(renderedType)) {
            //out.push_back('>');
            // for (unsigned i = 0; i < childrenToAdd_.size(); ++i)
            //     childrenToAdd_[i].child->asHTML(out, javaScript, timeouts);

            // out << innerHTML; // for WPushButton must be after childrenToAdd_

            // out << childrenHtml_.str();


            // auto ccc = std::tie(*this, out, javaScript, timeouts);
            // fmt::format_to(std::back_inserter(out), ">{}", ccc);

            // fmt::format_to(std::back_inserter(out), ">{}{}{}{}</{}>", ccc, innerHTML_, childrenHtml_.str(),
            //                           renderedType == DomElementType::DIV
            //                        && app->environment().agent() == UserAgent::IE6
            //                        && innerHTML_.empty()
            //                        && childrenToAdd_.empty()
            //                        && childrenHtml_.empty() ? "&nbsp;" : "", // IE6 will incorrectly set the height of empty divs
            //                renderedType  == DomElementType::OTHER ?
            //                    elementTagName_ : elementNames_[static_cast<unsigned int>(renderedType)]);

            // // IE6 will incorrectly set the height of empty divs
            // if (renderedType == DomElementType::DIV
            //     && app->environment().agent() == UserAgent::IE6
            //     && innerHTML.empty()
            //     && childrenToAdd_.empty()
            //     && childrenHtml_.empty())
            //     out.append("&nbsp;");
            // if (renderedType  == DomElementType::OTHER) // Custom tag name
            //     out << "</" << elementTagName_ << ">";
            // else
            //     out << "</" << elementNames_[static_cast<unsigned int>(renderedType)]
            //         << ">";
        // } else
        //     out.append(" />");

        // if (needButtonWrap_ /*&& supportButton*/)
        //     out.append("</button>");
        // else if (needAnchorWrap)
        //     out.append("</a>");
    //}

    //javaScript << javaScriptEvenWhenDeleted_ << javaScript_;
    fmt::format_to(std::back_inserter(javaScript), FMT_COMPILE("{}{}"), javaScriptEvenWhenDeleted_, std::string_view(javaScript_));

    if (timeOut_ != -1)
        timeouts.push_back(TimeoutEvent(timeOut_, id_, timeOutJSRepeat_));

    Utils::insert(timeouts, timeouts_);

}

void DomElement::asHTML(EscapeOStream& out,
                        EscapeOStream& javaScript,
                        std::vector<TimeoutEvent>& timeouts,
                        bool openingTagOnly) const
{
    if (mode_ != Mode::Create)
        throw WException("DomElement::asHTML() called with ModeUpdate");

    WApplication *app = WApplication::instance();
    processEvents(app);
    processProperties(app);

    auto clickEvent = eventHandlers_.find(WInteractWidget::CLICK_SIGNAL);

    bool needButtonWrap
        = (!app->environment().ajax()
           && (clickEvent != eventHandlers_.end())
           && (!clickEvent->second.signalName.empty())
           && (!app->environment().agentIsSpiderBot()));

    bool isSubmit = needButtonWrap;
    DomElementType renderedType = type_;

    if (needButtonWrap) {
        if (type_ == DomElementType::BUTTON) {
            /*
       * We don't need to wrap a button: we can just modify the attributes
       * type name and value. This avoid layout problems.
       *
       * Note that IE posts the button text instead of the value. We fix
       * this by encoding the value into the name.
       *
       * IE6 hell: IE will post all submit buttons, not just the one clicked.
       * We should therefore really be using input
       */
            DomElement *self = const_cast<DomElement *>(this);
            self->setAttribute("type", "submit");
            self->setAttribute("name", "signal=" + clickEvent->second.signalName);

            needButtonWrap = false;
        } else if (type_ == DomElementType::IMG) {
            /*
       * We don't need to wrap an image: we can substitute it for an input
       * type image. This avoid layout problems.
       */
            renderedType = DomElementType::INPUT;

            DomElement *self = const_cast<DomElement *>(this);
            self->setAttribute("type", "image");
            self->setAttribute("name", "signal=" + clickEvent->second.signalName);
            needButtonWrap = false;
        }
    }

    /*
   * We also should not wrap anchors, map area elements and form elements.
   */
    if (needButtonWrap) {
        if (   type_ == DomElementType::AREA
            || type_ == DomElementType::INPUT
            || type_ == DomElementType::SELECT)
            needButtonWrap = false;

        if (type_ == DomElementType::A) {
            std::string href = getAttribute("href");

            /*
           * If we're IE7/8 or there is a real URL, then we don't wrap
           */
            if (app->environment().agent() == UserAgent::IE7 ||
                app->environment().agent() == UserAgent::IE8 ||
                href.length() > 1)
                needButtonWrap = false;
            else if (app->theme()->canStyleAnchorAsButton()) {
                DomElement *self = const_cast<DomElement *>(this);
                self->setAttribute("href", app->url(app->internalPath())
                                               + "&signal=" + clickEvent->second.signalName);
                needButtonWrap = false;
            }
        } else if (type_ == DomElementType::AREA) {
            DomElement *self = const_cast<DomElement *>(this);
            self->setAttribute("href", app->url(app->internalPath())
                                           + "&signal=" + clickEvent->second.signalName);
        }
    }

    const bool supportButton = true;

    bool needAnchorWrap = false;

    if (!supportButton && type_ == DomElementType::BUTTON) {
        renderedType = DomElementType::INPUT;

        DomElement *self = const_cast<DomElement *>(this);
        if (!isSubmit)
            self->setAttribute("type", "button");
        self->setAttribute("value",
                           properties_.find(Property::InnerHTML)->second);
        self->setProperty(Property::InnerHTML, "");
    }

#ifndef WT_TARGET_JAVA
    EscapeOStream attributeValues(out);
#else // WT_TARGET_JAVA
    EscapeOStream attributeValues = out.push();
#endif // WT_TARGET_JAVA
    attributeValues.pushEscape(EscapeOStream::HtmlAttribute);

    std::string style;

    if (needButtonWrap) {
        if (supportButton) {
            out << "<button type=\"submit\" name=\"signal=";
            out.append(clickEvent->second.signalName, attributeValues);
            out << "\" class=\"Wt-wrap ";

            PropertyMap::const_iterator l = properties_.find(Property::Class);
            if (l != properties_.end()) {
                out << l->second;
                PropertyMap& map = const_cast<PropertyMap&>(properties_);
                map.erase(Property::Class);
            }

            out << '"';

            std::string wrapStyle = cssStyle();
            if (!isDefaultInline()) {
                // Put display: block; first, because it might
                // still be overridden if a widget is set to be inlined,
                // but isn't inline by default.
                wrapStyle = "display: block;" + wrapStyle;
            }

            if (!wrapStyle.empty()) {
                out << " style=";
                fastHtmlAttributeValue(out, attributeValues, wrapStyle);
            }

            PropertyMap::const_iterator i = properties_.find(Property::Disabled);
            if ((i != properties_.end()) && (i->second=="true"))
                out << " disabled=\"disabled\"";

            for (AttributeMap::const_iterator j = attributes_.begin();
                 j != attributes_.end(); ++j)
                if (j->first == "title") {
                    out << ' ' << j->first << '=';
                    fastHtmlAttributeValue(out, attributeValues, j->second.value);
                }

            if (app->environment().agent() != UserAgent::Konqueror
                && !app->environment().agentIsWebKit()
                && !app->environment().agentIsIE())
                style = "margin: 0px -3px -2px -3px;";

            out << "><" << elementNames_[static_cast<unsigned int>(renderedType)];
        } else {
            if (type_ == DomElementType::IMG)
                out << "<input type=\"image\"";
            else
                out << "<input type=\"submit\"";

            out << " name=";
            fastHtmlAttributeValue(out, attributeValues,
                                   "signal=" + clickEvent->second.signalName);
            out << " value=";

            PropertyMap::const_iterator i = properties_.find(Property::InnerHTML);
            if (i != properties_.end())
                fastHtmlAttributeValue(out, attributeValues, i->second);
            else
                out << "\"\"";
        }
    } else if (needAnchorWrap) {
        out << "<a href=\"#\" class=\"Wt-wrap\" onclick=";
        fastHtmlAttributeValue(out, attributeValues, clickEvent->second.jsCode);
        out << "><" << elementNames_[static_cast<unsigned int>(renderedType)];
    } else if (renderedType == DomElementType::OTHER)  // Custom tag name
        out << '<' << elementTagName_;
    else
        out << '<' << elementNames_[static_cast<unsigned int>(renderedType)];

    if (!id_.empty()) {
        out << " id=";
        fastHtmlAttributeValue(out, attributeValues, id_);
    }

    for (AttributeMap::const_iterator i = attributes_.begin();
         i != attributes_.end(); ++i)
        if (!app->environment().agentIsSpiderBot() || i->first != "name") {
            out << ' ' << i->first << '=';
            fastHtmlAttributeValue(out, attributeValues, i->second.value);
        }

    if (app->environment().ajax()) {
        for (EventHandlerMap::const_iterator i = eventHandlers_.begin();
             i != eventHandlers_.end(); ++i) {
            if (!i->second.jsCode.empty()) {
                if (globalUnfocused_
                    || (i->first == WInteractWidget::WHEEL_SIGNAL &&
                        app->environment().agentIsIE() &&
                        static_cast<unsigned int>(app->environment().agent()) >=
                            static_cast<unsigned int>(UserAgent::IE9)))
                    setJavaScriptEvent(javaScript, i->first, i->second, app);
                else {
                    out << " on" << const_cast<char *>(i->first) << '=';
                    fastHtmlAttributeValue(out, attributeValues, i->second.jsCode);
                }
            }
        }
    }

    std::string innerHTML = "";

    for (PropertyMap::const_iterator i = properties_.begin();
         i != properties_.end(); ++i) {
        switch (i->first) {
        case Property::InnerHTML:
            innerHTML += i->second; break;
        case Property::Disabled:
            if (i->second == "true")
                out << " disabled=\"disabled\"";
            break;
        case Property::ReadOnly:
            if (i->second == "true")
                out << " readonly=\"readonly\"";
            break;
        case Property::TabIndex:
            out << " tabindex=\"" << i->second << '"';
            break;
        case Property::Checked:
            if (i->second == "true")
                out << " checked=\"checked\"";
            break;
        case Property::Selected:
            if (i->second == "true")
                out << " selected=\"selected\"";
            break;
        case Property::SelectedIndex:
            if (i->second == "-1") {
                DomElement *self = const_cast<DomElement *>(this);
                self->callMethod("selectedIndex=-1");
            }
            break;
        case Property::Multiple:
            if (i->second == "true")
                out << " multiple=\"multiple\"";
            break;
        case Property::Target:
            out << " target=\"" << i->second << "\"";
            break;
        case Property::Download:
            out << " download=\"" << i->second << "\"";
            break;
        case Property::Indeterminate:
            if (i->second == "true") {
                DomElement *self = const_cast<DomElement *>(this);
                self->callMethod("indeterminate=" + i->second);
            }
            break;
        case Property::Value:
            if (type_ != DomElementType::TEXTAREA) {
                out << " value=";
                fastHtmlAttributeValue(out, attributeValues, i->second);
            } else {
                std::string v = i->second;
                innerHTML += WWebWidget::escapeText(v, false);
            }
            break;
        case Property::Src:
            out << " src=";
            fastHtmlAttributeValue(out, attributeValues, i->second);
            break;
        case Property::ColSpan:
            out << " colspan=";
            fastHtmlAttributeValue(out, attributeValues, i->second);
            break;
        case Property::RowSpan:
            out << " rowspan=";
            fastHtmlAttributeValue(out, attributeValues, i->second);
            break;
        case Property::Class:
            out << " class=";
            fastHtmlAttributeValue(out, attributeValues, i->second);
            break;
        case Property::Label:
            out << " label=";
            fastHtmlAttributeValue(out, attributeValues, i->second);
            break;
        case Property::Placeholder:
            out << " placeholder=";
            fastHtmlAttributeValue(out, attributeValues, i->second);
            break;
        default:
            break;
        }
    }

    if (!needButtonWrap)
        style += cssStyle();

    if (!style.empty()) {
        out << " style=";
        fastHtmlAttributeValue(out, attributeValues, style);
    }

    if (needButtonWrap && !supportButton)
        out << " />";
    else {
        if (openingTagOnly) {
            out << '>';
            return;
        }

        /*
     * http://www.w3.org/TR/html/#guidelines
     * XHTML recommendation, back-wards compatibility with HTML: C.2, C.3:
     * do not use minimized forms when content is empty like <p />, and use
     * minimized forms for certain elements like <br />
     */
        if (!isSelfClosingTag(renderedType)) {
            out << '>';
            for (unsigned i = 0; i < childrenToAdd_.size(); ++i)
                childrenToAdd_[i].child.asHTML(out, javaScript, timeouts);

            out << innerHTML; // for WPushButton must be after childrenToAdd_

            out << fmt::to_string(childrenHtml_);

            // IE6 will incorrectly set the height of empty divs
            if (renderedType == DomElementType::DIV
                && app->environment().agent() == UserAgent::IE6
                && innerHTML.empty()
                && childrenToAdd_.empty()
                && !childrenHtml_.size())
                out << "&nbsp;";
            if (renderedType  == DomElementType::OTHER) // Custom tag name
                out << "</" << elementTagName_ << ">";
            else
                out << "</" << elementNames_[static_cast<unsigned int>(renderedType)]
                    << ">";
        } else
            out << " />";

        if (needButtonWrap && supportButton)
            out << "</button>";
        else if (needAnchorWrap)
            out << "</a>";
    }

    javaScript << javaScriptEvenWhenDeleted_ << javaScript3_;

    if (timeOut_ != -1)
        timeouts.push_back(TimeoutEvent(timeOut_, id_, timeOutJSRepeat_));

    Utils::insert(timeouts, timeouts_);
}

std::string DomElement::createVar() const
{
#ifndef WT_TARGET_JAVA
    fmt::format_to(std::back_inserter(var_), FMT_COMPILE("j{}"), nextId_++);
  // char buf[20];
  // std::sprintf(buf, "j%u", nextId_++);
  // var_ = buf;
#else // !WT_TARGET_JAVA
  var_ = "j" + std::to_string(nextId_++);
#endif // !WT_TARGET_JAVA

  return var_;
}

void DomElement::declare(EscapeOStream& out) const
{
  if (var_.empty())
    out << "var " << createVar() << "=" WT_CLASS ".$('" << id_ << "');\n";
}

void DomElement::declare(fmt::memory_buffer &out) const
{
    if (var_.empty())
        fmt::format_to(std::back_inserter(out), FMT_COMPILE("var {}={}.$('{}');\n"), createVar(), WT_CLASS, id_);
}

bool DomElement::canWriteInnerHTML(WApplication *app) const
{
  /*
   * http://lists.apple.com/archives/web-dev/2004/Apr/msg00122.html
   * "The problem is not that innerHTML doesn't work (it works fine),
   *  but that Safari can't handle writing the innerHTML of a <tbody> tag.
   *  If you write the entire table (including <table> and <tbody>) in the
   *  innerHTML string it works fine.
   */
  /* http://msdn.microsoft.com/workshop/author/tables/buildtables.asp
   * Note When using Dynamic HTML (DHTML) to create a document, you can 
   * create objects and set the innerText or innerHTML property of the object.
   * However, because of the specific structure required by tables,
   * the innerText and innerHTML properties of the table and tr objects are
   * read-only.
   */
  /* http://support.microsoft.com/kb/276228
   * BUG: Internet Explorer Fails to Set the innerHTML Property of the
   * SelectionFlag::Select Object. Seems to affect at least up to IE6.0
   */
  if ((app->environment().agentIsIE()
       || app->environment().agent() == UserAgent::Konqueror)
      && (   type_ == DomElementType::TBODY
	  || type_ == DomElementType::THEAD
	  || type_ == DomElementType::TABLE
	  || type_ == DomElementType::COLGROUP
	  || type_ == DomElementType::TR
	  || type_ == DomElementType::SELECT
	  || type_ == DomElementType::TD
	  || type_ == DomElementType::OPTGROUP))
    return false;

  return true;
}

#if 0
bool DomElement::containsElement(DomElementType type) const
{
  for (unsigned i = 0; i < childrenToAdd_.size(); ++i) {
    if (childrenToAdd_[i].child->type_ == type)
      return true;
    if (childrenToAdd_[i].child->containsElement(type))
      return true;
  }

  return false;
}
#endif

void DomElement::asJavaScript(WStringStream& out)
{
  mode_ = Mode::Update;

  EscapeOStream eout(out);

  declare(eout);
  eout << var_ << ".setAttribute('id', '" << id_ << "');\n";

  mode_ = Mode::Create;

  setJavaScriptProperties(eout, WApplication::instance());
  setJavaScriptAttributes(eout);
  asJavaScript(eout, Priority::Update);
}

void DomElement::asJavaScript(fmt::memory_buffer &out)
{
    mode_ = Mode::Update;


    declare(out);
    fmt::format_to(std::back_inserter(out), FMT_COMPILE("{}.setAttribute('id', '{}');\n"), var_, id_);
    //eout << var_ << ".setAttribute('id', '" << id_ << "');\n";

    mode_ = Mode::Create;

    setJavaScriptProperties(out, WApplication::instance());
    setJavaScriptAttributes(out);
    asJavaScript(out, Priority::Update);
}

std::string DomElement::asJavaScript(fmt::memory_buffer &out, Priority priority) const
{
    switch(priority) {
    case Priority::Delete:

        //if (!javaScriptEvenWhenDeleted_.empty() || (removeAllChildren_ >= 0)) {
            //out << javaScriptEvenWhenDeleted_;
            if (removeAllChildren_ >= 0) {
                declare(out);
                if (removeAllChildren_ == 0)
                    fmt::format_to(std::back_inserter(out), FMT_COMPILE("{}{}.setHtml({}, '');\n"), javaScriptEvenWhenDeleted_, WT_CLASS, var_);
                    //out << WT_CLASS << ".setHtml(" << var_ << ", '');\n";
                else {
                    fmt::format_to(std::back_inserter(out), FMT_COMPILE("{}(Array.from({}.querySelectorAll(':scope > *')).slice({})).forEach( elem => elem.remove());"),
                                   javaScriptEvenWhenDeleted_,
                                   var_,
                                   removeAllChildren_);
                    // out << "(Array.from(" << var_ << ".querySelectorAll(':scope > *')).slice(" << removeAllChildren_
                    //     << ")).forEach( elem => elem.remove());";
                }
            }
            else
                fmt::format_to(std::back_inserter(out), "{}", javaScriptEvenWhenDeleted_);
        //}

        return var_;
    case Priority::Create:
        if (mode_ == Mode::Create) {
            if (!id_.empty())
                fmt::format_to(std::back_inserter(out), FMT_COMPILE("{}.setAttribute('id', '{}');\n"), var_, id_);
                //out << var_ << ".setAttribute('id', '" << id_ << "');\n";

            setJavaScriptAttributes(out);
            setJavaScriptProperties(out, WApplication::instance());
        }

        return var_;
    case Priority::Update:
    {
        WApplication *app = WApplication::instance();

        bool childrenUpdated = false;

        /*
         * short-cut for frequent short manipulations
         */
        if (mode_ == Mode::Update && numManipulations_ == 1) {
            for (unsigned i = 0; i < updatedChildren_.size(); ++i) {
                const DomElement &child = updatedChildren_[i];
                child.asJavaScript(out, Priority::Update);
            }

            childrenUpdated = true;

            if (auto it = properties_.find(Property::StyleDisplay); it != properties_.end())
            {
                std::string style = properties_.find(Property::StyleDisplay)->second;
                if (it->second == "none") {
                    fmt::format_to(std::back_inserter(out), FMT_COMPILE(WT_CLASS ".hide('{}');\n"), id_);
                    //out << WT_CLASS ".hide('" << id_ << "');\n";
                    return var_;
                } else if (it->second == "inline") {
                    fmt::format_to(std::back_inserter(out), FMT_COMPILE(WT_CLASS ".inline('{}');\n"), id_);
                    //out << WT_CLASS ".inline('" + id_ + "');\n";
                    return var_;
                } else if (it->second == "block") {
                    fmt::format_to(std::back_inserter(out), FMT_COMPILE(WT_CLASS ".block('{}');\n"), id_);
                    //out << WT_CLASS ".block('" + id_ + "');\n";
                    return var_;
                } else {
                    fmt::format_to(std::back_inserter(out), FMT_COMPILE(WT_CLASS ".show('{}','{}');\n"), id_, it->second);
                    //out << WT_CLASS ".show('" << id_ << "', '" << style << "');\n";
                    return var_;
                }
            } else if (javaScript_.size()) {
                fmt::format_to(std::back_inserter(out), "{}", std::string_view(javaScript_.data(), javaScript_.size()));
                return var_;
            }
        }

        if (unwrapped_)
            fmt::format_to(std::back_inserter(out), FMT_COMPILE(WT_CLASS ".unwrap('{}');\n"), id_);
            //out << WT_CLASS ".unwrap('" << id_ << "');\n";

        processEvents(app);
        processProperties(app);

        if (replaced_) {
            declare(out);

            std::string varr = replaced_->createVar();
            // WStringStream insertJs;
            // insertJs << var_ << ".parentNode.replaceChild("
            //          << varr << ',' << var_ << ");\n";
            // replaced_->createElement(out, app, insertJs.str());
            // if (unstubbed_)
            //     out << WT_CLASS ".unstub(" << var_ << ',' << varr << ','
            //         << (hideWithDisplay_ ? 1 : 0) << ");\n";

            std::string insertJs;
            fmt::format_to(std::back_inserter(insertJs), FMT_COMPILE("{}.parentNode.replaceChild({},{});\n"), var_, varr, var_);
            replaced_->createElement(out, app, insertJs);
            if (unstubbed_)
                fmt::format_to(std::back_inserter(out), FMT_COMPILE(WT_CLASS ".unstub({},{});\n"), var_, varr, (hideWithDisplay_ ? 1 : 0));


            return var_;
        } else if (insertBefore_) {
            declare(out);

            std::string varr = insertBefore_->createVar();
            // WStringStream insertJs;
            // insertJs << var_ << ".parentNode.insertBefore(" << varr << ","
            //          << var_ + ");\n";
            auto insertJs = fmt::format( FMT_COMPILE("{}.parentNode.insertBefore({},{});\n"), var_, varr, var_);
            insertBefore_->createElement(out, app, insertJs);

            return var_;
        }

        // FIXME optimize with subselect

        if (!childrenToSave_.empty()) {
            declare(out);
            fmt::format_to(std::back_inserter(out),  FMT_COMPILE(WT_CLASS ".saveReparented({});\n"), var_);
            //out << WT_CLASS << ".saveReparented(" << var_ << ");";
        }

        for (unsigned i = 0; i < childrenToSave_.size(); ++i) {
            if (app->environment().agentIsIE())
                fmt::format_to(std::back_inserter(out),  FMT_COMPILE("var c{}{}=$('#').detach();"), var_, i, childrenToSave_[i]);
            else
                fmt::format_to(std::back_inserter(out),  FMT_COMPILE("var c{}{}=$('#');"), var_, i, childrenToSave_[i]);
            // out << "var c" << var_ << (int)i << '='
            //     << "$('#" << childrenToSave_[i] << "')";
            // // In IE, contents is deleted by setting innerHTML
            // if (app->environment().agentIsIE())
            //     out << ".detach()";
            // out << ";";
        }

        if (mode_ != Mode::Create) {
            setJavaScriptProperties(out, app);
            setJavaScriptAttributes(out);
        }

        for (auto i = eventHandlers_.begin(); i != eventHandlers_.end(); ++i)
            if ((mode_ == Mode::Update) || !i->second.jsCode.empty())
                setJavaScriptEvent(out, i->first, i->second, app);

        renderInnerHtmlJS(out, app);

        for (unsigned i = 0; i < childrenToSave_.size(); ++i)
            fmt::format_to(std::back_inserter(out),  FMT_COMPILE(WT_CLASS ".replaceWith('{}',c{}{});"), var_, i, childrenToSave_[i]);

            // out << WT_CLASS ".replaceWith('" << childrenToSave_[i] << "',c"
            //     << var_ << (int)i << ");";

        // Fix for http://redmine.emweb.be/issues/1847: custom JS
        // won't find objects that still have to be moved in place
        renderDeferredJavaScript(out);

        if (!childrenUpdated)
            for (unsigned i = 0; i < updatedChildren_.size(); ++i) {
                const DomElement &child = updatedChildren_[i];
                child.asJavaScript(out, Priority::Update);
            }

        return var_;
    }
    }

    return var_;
}


void DomElement::createTimeoutJs(WStringStream& out,
				 const TimeoutList& timeouts, WApplication *app)
{
  for (unsigned i = 0; i < timeouts.size(); ++i)
    out << app->javaScriptClass()
	<< "._p_.addTimerEvent('" << timeouts[i].event << "', " 
	<< timeouts[i].msec << ","
	<< timeouts[i].repeat << ");\n";
}

void DomElement::createTimeoutJs(fmt::memory_buffer &out, const TimeoutList &timeouts, WApplication *app)
{
    fmt::format_to(std::back_inserter(out), "{}", fmt::join(timeouts, ""));
}

void DomElement::createElement(WStringStream& out, WApplication *app,
			       const std::string& domInsertJS)
{
  EscapeOStream sout(out);
  createElement(sout, app, domInsertJS);
}

void DomElement::createElement(EscapeOStream& out, WApplication *app,
                               const std::string& domInsertJS)
{
    if (var_.empty())
        createVar();

    out << "var " << var_ << "=";

    if (app->environment().agentIsIE()
        && app->environment().agent() <= UserAgent::IE8
        && type_ != DomElementType::TEXTAREA) {
        /*
         * IE pre 9 can create the entire opening tag at once.
         * This rocks because it results in fewer JavaScript statements.
         * It also avoids problems with changing certain attributes not
         * working in IE.
         *
         * However, we cannot do it for TEXTAREA since there are inconsistencies
         * with setting its value
         */
        out << "document.createElement('";
        out.pushEscape(EscapeOStream::JsStringLiteralSQuote);
        TimeoutList timeouts;
        EscapeOStream dummy;
        asHTML(out, dummy, timeouts, true);
        out.popEscape();
        out << "');";
        out << domInsertJS;
        renderInnerHtmlJS(out, app);
        renderDeferredJavaScript(out);
    } else {
        out << "document.createElement('"
            << elementNames_[static_cast<unsigned int>(type_)] << "');";
        out << domInsertJS;
        asJavaScript(out, Priority::Create);
        asJavaScript(out, Priority::Update);
    }
}
void DomElement::createElement(fmt::memory_buffer &out, WApplication *app, std::string_view domInsertJS)
{
    if (var_.empty())
        createVar();



    if (app->environment().agentIsIE()
        && app->environment().agent() <= UserAgent::IE8
        && type_ != DomElementType::TEXTAREA) {
        /*
     * IE pre 9 can create the entire opening tag at once.
     * This rocks because it results in fewer JavaScript statements.
     * It also avoids problems with changing certain attributes not
     * working in IE.
     *
     * However, we cannot do it for TEXTAREA since there are inconsistencies
     * with setting its value
     */
        fmt::format_to(std::back_inserter(out),  FMT_COMPILE("var {}=document.createElement('"), var_);
        // out << "document.createElement('";
        // out.pushEscape(EscapeOStream::JsStringLiteralSQuote);
        TimeoutList timeouts;
        fmt::memory_buffer dummy;
        asHTML(out, dummy, timeouts, true);
        //out.popEscape();
        //out << "');";
        //out << domInsertJS;
        fmt::format_to(std::back_inserter(out),  FMT_COMPILE("');{}"), domInsertJS);
        renderInnerHtmlJS(out, app);
        renderDeferredJavaScript(out);
    } else {
        fmt::format_to(std::back_inserter(out),  FMT_COMPILE("var {}=document.createElement('{}');{}"),
                       var_,
                       elementNames_[static_cast<unsigned int>(type_)],
                       domInsertJS);
        // out << "document.createElement('"
        //     << elementNames_[static_cast<unsigned int>(type_)] << "');";
        // out << domInsertJS;
        asJavaScript(out, Priority::Create);
        asJavaScript(out, Priority::Update);
    }
}

std::string DomElement::addToParent(fmt::memory_buffer &out, const std::string &parentVar, int pos, WApplication *app)
{
    createVar();

    if (type_ == DomElementType::TD || type_ == DomElementType::TR) {
        if (type_ == DomElementType::TD)
            fmt::format_to(std::back_inserter(out), FMT_COMPILE("var {}={}.insertCell({});\n"), var_, parentVar, pos);
        else
            fmt::format_to(std::back_inserter(out), FMT_COMPILE("var {}={}.insertRow({});\n"), var_, parentVar, pos);

        asJavaScript(out, Priority::Create);
        asJavaScript(out, Priority::Update);
    } else {
        fmt::memory_buffer insertJS;
        if (pos != -1)
            fmt::format_to(std::back_inserter(insertJS), FMT_COMPILE(WT_CLASS ".insertAt({},{},{});"), parentVar, var_, pos);
        else
            fmt::format_to(std::back_inserter(insertJS), FMT_COMPILE("{}.appendChild({});"), parentVar, var_);

        createElement(out, app, std::string_view(insertJS.data(), insertJS.size()));
    }

    return var_;
}


std::string DomElement::addToParent(WStringStream& out,
                                    const std::string& parentVar,
                                    int pos, WApplication *app)
{
    EscapeOStream sout(out);
    return addToParent(sout, parentVar, pos, app);
}

std::string DomElement::addToParent(EscapeOStream& out,
                                    const std::string& parentVar,
                                    int pos, WApplication *app)
{
  createVar();

  if (type_ == DomElementType::TD || type_ == DomElementType::TR) {
    out << "var " << var_ << "=";

    if (type_ == DomElementType::TD)
      out << parentVar << ".insertCell(" << pos << ");\n";
    else
      out << parentVar << ".insertRow(" << pos << ");\n";

    asJavaScript(out, Priority::Create);
    asJavaScript(out, Priority::Update);
  } else {
    WStringStream insertJS;
    if (pos != -1)
      insertJS << WT_CLASS ".insertAt(" << parentVar << "," << var_
	       << "," << pos << ");";
    else
      insertJS << parentVar << ".appendChild(" << var_ << ");\n";

    createElement(out, app, insertJS.str());
  }

  return var_;
}

std::string DomElement::asJavaScript(EscapeOStream& out, Priority priority) const
{
    switch(priority) {
    case Priority::Delete:
        if (!javaScriptEvenWhenDeleted_.empty() || (removeAllChildren_ >= 0)) {
            out << javaScriptEvenWhenDeleted_;
            if (removeAllChildren_ >= 0) {
                declare(out);
                if (removeAllChildren_ == 0)
                    out << WT_CLASS << ".setHtml(" << var_ << ", '');\n";
                else {
                    out << "(Array.from(" << var_ << ".querySelectorAll(':scope > *')).slice(" << removeAllChildren_
                        << ")).forEach( elem => elem.remove());";
                }
            }
        }

        return var_;
    case Priority::Create:
        if (mode_ == Mode::Create) {
            if (!id_.empty())
                out << var_ << ".setAttribute('id', '" << id_ << "');\n";

            setJavaScriptAttributes(out);
            setJavaScriptProperties(out, WApplication::instance());
        }

        return var_;
    case Priority::Update:
    {
        WApplication *app = WApplication::instance();

        bool childrenUpdated = false;

        /*
     * short-cut for frequent short manipulations
     */
        if (mode_ == Mode::Update && numManipulations_ == 1) {
            for (unsigned i = 0; i < updatedChildren_.size(); ++i) {
                const DomElement &child = updatedChildren_[i];
                child.asJavaScript(out, Priority::Update);
            }

            childrenUpdated = true;

            if (properties_.find(Property::StyleDisplay) != properties_.end()) {
                std::string style = properties_.find(Property::StyleDisplay)->second;
                if (style == "none") {
                    out << WT_CLASS ".hide('" << id_ << "');\n";
                    return var_;
                } else if (style == "inline") {
                    out << WT_CLASS ".inline('" + id_ + "');\n";
                    return var_;
                } else if (style == "block") {
                    out << WT_CLASS ".block('" + id_ + "');\n";
                    return var_;
                } else {
                    out << WT_CLASS ".show('" << id_ << "', '" << style << "');\n";
                    return var_;
                }
            } else if (!javaScript3_.empty()) {
                out << javaScript3_;
                return var_;
            }
        }

        if (unwrapped_)
            out << WT_CLASS ".unwrap('" << id_ << "');\n";

        processEvents(app);
        processProperties(app);

        if (replaced_) {
            declare(out);

            std::string varr = replaced_->createVar();
            WStringStream insertJs;
            insertJs << var_ << ".parentNode.replaceChild("
                     << varr << ',' << var_ << ");\n";
            replaced_->createElement(out, app, insertJs.str());
            if (unstubbed_)
                out << WT_CLASS ".unstub(" << var_ << ',' << varr << ','
                    << (hideWithDisplay_ ? 1 : 0) << ");\n";

            return var_;
        } else if (insertBefore_) {
            declare(out);

            std::string varr = insertBefore_->createVar();
            WStringStream insertJs;
            insertJs << var_ << ".parentNode.insertBefore(" << varr << ","
                     << var_ + ");\n";
            insertBefore_->createElement(out, app, insertJs.str());

            return var_;
        }

        // FIXME optimize with subselect

        if (!childrenToSave_.empty()) {
            declare(out);
            out << WT_CLASS << ".saveReparented(" << var_ << ");";
        }

        for (unsigned i = 0; i < childrenToSave_.size(); ++i) {
            out << "var c" << var_ << (int)i << '='
                << "$('#" << childrenToSave_[i] << "')";
            // In IE, contents is deleted by setting innerHTML
            if (app->environment().agentIsIE())
                out << ".detach()";
            out << ";";
        }

        if (mode_ != Mode::Create) {
            setJavaScriptProperties(out, app);
            setJavaScriptAttributes(out);
        }

        for (EventHandlerMap::const_iterator i = eventHandlers_.begin();
             i != eventHandlers_.end(); ++i)
            if ((mode_ == Mode::Update) || !i->second.jsCode.empty())
                setJavaScriptEvent(out, i->first, i->second, app);

        renderInnerHtmlJS(out, app);

        for (unsigned i = 0; i < childrenToSave_.size(); ++i)
            out << WT_CLASS ".replaceWith('" << childrenToSave_[i] << "',c"
                << var_ << (int)i << ");";

        // Fix for http://redmine.emweb.be/issues/1847: custom JS
        // won't find objects that still have to be moved in place
        renderDeferredJavaScript(out);

        if (!childrenUpdated)
            for (unsigned i = 0; i < updatedChildren_.size(); ++i) {
                const DomElement &child = updatedChildren_[i];
                child.asJavaScript(out, Priority::Update);
            }

        return var_;
    }
    }

    return var_;
}

bool DomElement::willRenderInnerHtmlJS(WApplication *app) const
{
  /*
   * Returns whether we will (or at least can) write the
   * innerHTML with setHtml(), combining children and literal innerHTML
   */
  return childrenHtml_.size() || (wasEmpty_ && canWriteInnerHTML(app));
}

void DomElement::renderInnerHtmlJS(EscapeOStream& out, WApplication *app) const
{
    if (willRenderInnerHtmlJS(app)) {
        std::string innerHTML;

        if (!properties_.empty()) {
            if (auto i = properties_.find(Property::InnerHTML); i != properties_.end()) {
                innerHTML += i->second;
            }
            if (auto i = properties_.find(Property::AddedInnerHTML); i != properties_.end()) {
                innerHTML += i->second;
            }
        }

        /*
         * Do we actually have anything to render ?
         *   first condition: for IE6: write &nbsp; inside a empty <div></div>
         */
        if ((type_ == DomElementType::DIV
             && app->environment().agent() == UserAgent::IE6)
            || !childrenToAdd_.empty() || childrenHtml_.size()
            || !innerHTML.empty()) {
            declare(out);

            out << WT_CLASS ".setHtml(" << var_ << ",'";

            out.pushEscape(EscapeOStream::JsStringLiteralSQuote);
            TimeoutList timeouts;
            EscapeOStream js;

            for (unsigned i = 0; i < childrenToAdd_.size(); ++i)
                childrenToAdd_[i].child.asHTML(out, js, timeouts);

            out << innerHTML;

            out << fmt::to_string(childrenHtml_);

            if (type_ == DomElementType::DIV
                && app->environment().agent() == UserAgent::IE6
                && childrenToAdd_.empty()
                && innerHTML.empty()
                && !childrenHtml_.size())
                out << "&nbsp;";

            out.popEscape();

            out << "');\n";

            Utils::insert(timeouts, timeouts_);

            for (unsigned i = 0; i < timeouts.size(); ++i) {
                out << app->javaScriptClass()
                << "._p_.addTimerEvent('" << timeouts[i].event << "', "
                << timeouts[i].msec << ','
                << timeouts[i].repeat << ");\n";
            }

            out << js;
        }
    } else {
        for (unsigned i = 0; i < childrenToAdd_.size(); ++i) {
            declare(out);
            DomElement &child = (DomElement&)childrenToAdd_[i].child;
            child.addToParent(out, var_, childrenToAdd_[i].pos, app);
        }
    }

    if (timeOut_ != -1) {
        out << app->javaScriptClass() << "._p_.addTimerEvent('"
            << id_ << "', " << timeOut_ << ','
            << timeOutJSRepeat_ << ");\n";
    }
}
void DomElement::renderInnerHtmlJS(fmt::memory_buffer &out, WApplication *app) const
{
    if (willRenderInnerHtmlJS(app)) {
        std::string innerHTML;

        if (!properties_.empty()) {
            if (auto i = properties_.find(Property::InnerHTML); i != properties_.end()) {
                innerHTML += i->second;
            }
            if (auto i = properties_.find(Property::AddedInnerHTML); i != properties_.end()) {
                innerHTML += i->second;
            }
        }

        /*
         * Do we actually have anything to render ?
         *   first condition: for IE6: write &nbsp; inside a empty <div></div>
         */
        if ((type_ == DomElementType::DIV
             && app->environment().agent() == UserAgent::IE6)
            || !childrenToAdd_.empty() || childrenHtml_.size()
            || !innerHTML.empty()) {
            declare(out);

            TimeoutList timeouts;
            fmt::memory_buffer js;

            auto tuple = std::tie(*this, out, js, timeouts);

            fmt::format_to(std::back_inserter(out), FMT_COMPILE(WT_CLASS ".setHtml({},'{}{}{}');\n"), var_, tuple, innerHTML, std::string_view(childrenHtml_));
            //out << WT_CLASS ".setHtml(" << var_ << ",'";

            //out.pushEscape(EscapeOStream::JsStringLiteralSQuote);
            // TimeoutList timeouts;
            // EscapeOStream js;

            // for (unsigned i = 0; i < childrenToAdd_.size(); ++i)
            //     childrenToAdd_[i].child->asHTML(out, js, timeouts);

            //out << innerHTML;

            //out << childrenHtml_.str();

            //deprecated
            // if (type_ == DomElementType::DIV
            //     && app->environment().agent() == UserAgent::IE6
            //     && childrenToAdd_.empty()
            //     && innerHTML.empty()
            //     && childrenHtml_.empty())
            //     out << "&nbsp;";

            //out.popEscape();

            //out << "');\n";

            Utils::insert(timeouts, timeouts_);

            fmt::format_to(std::back_inserter(out),  FMT_COMPILE("{}{}"), fmt::join(timeouts, ""), std::string_view(js.data(), js.size()));

            // for (unsigned i = 0; i < timeouts.size(); ++i) {
            //     out << app->javaScriptClass()
            //     << "._p_.addTimerEvent('" << timeouts[i].event << "', "
            //     << timeouts[i].msec << ','
            //     << timeouts[i].repeat << ");\n";
            // }

            // out << js;
        }
    } else {
        declare(out);
        for (unsigned i = 0; i < childrenToAdd_.size(); ++i) {
            DomElement &child = (DomElement&)childrenToAdd_[i].child;
            child.addToParent(out, var_, childrenToAdd_[i].pos, app);
        }
    }

    if (timeOut_ != -1) {
        fmt::format_to(std::back_inserter(out),  FMT_COMPILE("{}._p_.addTimerEvent('{}',{},{});\n"),
                       app->javaScriptClass(),
                       id_,
                       timeOut_,
                       timeOutJSRepeat_);
        // out << app->javaScriptClass() << "._p_.addTimerEvent('"
        //     << id_ << "', " << timeOut_ << ','
        //     << timeOutJSRepeat_ << ");\n";
    }
}

void DomElement::renderDeferredJavaScript(fmt::memory_buffer &out) const
{
    if (javaScript_.size()) {
        declare(out);
        fmt::format_to(std::back_inserter(out), "{}\n", javaScript_);
        // out.append(javaScript_);
        // out.push_back('\n');
    }
}

void DomElement::renderDeferredJavaScript(EscapeOStream& out) const
{
  if (!javaScript3_.empty()) {
    declare(out);
    out << javaScript3_ << '\n';
  }
}

void DomElement::setJavaScriptProperties(fmt::memory_buffer &out, WApplication *app) const
{

    bool pushed = false;

    for (auto i = properties_.begin(); i != properties_.end(); ++i) {
        declare(out);

        switch(i->first) {
        case Property::InnerHTML:
        case Property::AddedInnerHTML:
            /*
           * In all cases, setJavaScriptProperties() is followed by
           * renderInnerHtmlJS() which also considers children.
           *
           * When there's 'AddedInnerHTML' then willRenderInnerHtmlJS() should
           * return false, and that's necessary since then we need to pass 'true'
           * as last argument to setHtml()
           */
            if (willRenderInnerHtmlJS(app))
                break;

            fmt::format_to(std::back_inserter(out),  FMT_COMPILE(WT_CLASS ".setHtml({},'{:s}',{});"),
                           var_,
                           JsString(i->second),
                           i->first == Property::InnerHTML);

            // out << WT_CLASS ".setHtml(" << var_ << ',';
            // if (!pushed) {
            //     escaped.pushEscape(EscapeOStream::JsStringLiteralSQuote);
            //     pushed = true;
            // }
            // fastJsStringLiteral(out, escaped, i->second);
            // if (i->first == Property::InnerHTML)
            //     out << ",false";
            // else
            //     out << ",true";

            // out << ");";

            break;
        case Property::Value:
            fmt::format_to(std::back_inserter(out),  FMT_COMPILE("{}.value='{:s}';"), var_, JsString(i->second));
            // out << var_ << ".value=";
            // if (!pushed) {
            //     escaped.pushEscape(EscapeOStream::JsStringLiteralSQuote);
            //     pushed = true;
            // }
            // fastJsStringLiteral(out, escaped, i->second);
            // out << ';';
            break;
        case Property::Target:
            fmt::format_to(std::back_inserter(out),  FMT_COMPILE("{}.target='{}';"), var_, i->second);
            //out << var_ << ".target='" << i->second << "';";
            break;
        case Property::Indeterminate:
            fmt::format_to(std::back_inserter(out),  FMT_COMPILE("{}.indeterminate={};"), var_, i->second);
            //out << var_ << ".indeterminate=" << i->second << ";";
            break;
        case Property::Disabled:
            if (type_ == DomElementType::A) {
                if (i->second == "true")
                    fmt::format_to(std::back_inserter(out), FMT_COMPILE("{}.setAttribute('disabled', 'disabled');"), var_);
                    //out << var_ << ".setAttribute('disabled', 'disabled');";
                else
                    fmt::format_to(std::back_inserter(out), FMT_COMPILE("{}.removeAttribute('disabled', 'disabled');"), var_);
                    //out << var_ << ".removeAttribute('disabled', 'disabled');";
            } else
                fmt::format_to(std::back_inserter(out), FMT_COMPILE("{}.disabled={};"), var_, i->second);
                //out << var_ << ".disabled=" << i->second << ';';
            break;
        case Property::ReadOnly:
            fmt::format_to(std::back_inserter(out), FMT_COMPILE("{}.readOnly={};"), var_, i->second);
            //out << var_ << ".readOnly=" << i->second << ';';
            break;
        case Property::TabIndex:
            fmt::format_to(std::back_inserter(out), FMT_COMPILE("{}.tabIndex={};"), var_, i->second);
            //out << var_ << ".tabIndex=" << i->second << ';';
            break;
        case Property::Checked:
            fmt::format_to(std::back_inserter(out), FMT_COMPILE("{}.checked={};"), var_, i->second);
            //out << var_ << ".checked=" << i->second << ';';
            break;
        case Property::Selected:
            fmt::format_to(std::back_inserter(out), FMT_COMPILE("{}.selected={};"), var_, i->second);
            //out << var_ << ".selected=" << i->second << ';';
            break;
        case Property::SelectedIndex:
            fmt::format_to(std::back_inserter(out), FMT_COMPILE("setTimeout(function(){{{}.selectedIndex={};}},0);"), var_, i->second);
            // out << "setTimeout(function() { "
            //     << var_ << ".selectedIndex=" << i->second << ";}, 0);";
            break;
        case Property::Multiple:
            fmt::format_to(std::back_inserter(out), FMT_COMPILE("{}.multiple={};"), var_, i->second);
            //out << var_ << ".multiple=" << i->second << ';';
            break;
        case Property::Src:
            fmt::format_to(std::back_inserter(out), FMT_COMPILE("{}.src={};"), var_, i->second);
            //out << var_ << ".src='" << i->second << "\';";
            break;
        case Property::ColSpan:
            fmt::format_to(std::back_inserter(out), FMT_COMPILE("{}.colSpan={};"), var_, i->second);
            //out << var_ << ".colSpan=" << i->second << ";";
            break;
        case Property::RowSpan:
            fmt::format_to(std::back_inserter(out), FMT_COMPILE("{}.rowSpan={};"), var_, i->second);
            //out << var_ << ".rowSpan=" << i->second << ";";
            break;
        case Property::Label:
            fmt::format_to(std::back_inserter(out), FMT_COMPILE("{}.label='{:s}';"), var_, JsString(i->second));
            //out << var_ << ".label=";
            // if (!pushed) {
            //     escaped.pushEscape(EscapeOStream::JsStringLiteralSQuote);
            //     pushed = true;
            // }
            // fastJsStringLiteral(out, escaped, i->second);
            // out << ';';
            break;
        case Property::Placeholder:
            fmt::format_to(std::back_inserter(out), FMT_COMPILE("{}.placeholder='{:s}';"), var_, JsString(i->second));
            //out << var_ << ".placeholder=";
            // if (!pushed) {
            //     escaped.pushEscape(EscapeOStream::JsStringLiteralSQuote);
            //     pushed = true;
            // }
            // fastJsStringLiteral(out, escaped, i->second);
            // out << ';';
            break;
        case Property::Class:
            fmt::format_to(std::back_inserter(out), FMT_COMPILE("{}.className='{:s}';"), var_, JsString(i->second));
            //out << var_ << ".className=";
            // if (!pushed) {
            //     escaped.pushEscape(EscapeOStream::JsStringLiteralSQuote);
            //     pushed = true;
            // }
            // fastJsStringLiteral(out, escaped, i->second);
            // out << ';';
            break;
        case Property::StyleFloat:

            //out << var_ << ".style.";
            if (app->environment().agentIsIE())
                fmt::format_to(std::back_inserter(out), FMT_COMPILE("{}.style.styleFloat=\'{}\';"), var_, i->second);
                //out << "styleFloat";
            else
                fmt::format_to(std::back_inserter(out), FMT_COMPILE("{}.style.cssFloat=\'{}\';"), var_, i->second);
                //out << "cssFloat";
            //out << "=\'" << i->second << "\';";
            break;
        case Wt::Property::StyleWidthExpression:
            fmt::format_to(std::back_inserter(out), FMT_COMPILE("{}.style.setExpression('width', '{:s}');"), var_, JsString(i->second));
            //out << var_ << ".style.setExpression('width',";
            // if (!pushed) {
            //     escaped.pushEscape(EscapeOStream::JsStringLiteralSQuote);
            //     pushed = true;
            // }
            // fastJsStringLiteral(out, escaped, i->second);
            // out << ");";
            break;
        default: {
            unsigned int p = static_cast<unsigned int>(i->first);
            if (p >= static_cast<unsigned int>(Property::Style) &&
                p < static_cast<unsigned int>(Property::LastPlusOne)) {
                if (app->environment().agent() == UserAgent::IE6) {
                    /*
                   * Unsupported properties, like min-height, would otherwise be
                   * ignored, but we want this information client-side. (Still, really ?)
                   */
                    fmt::format_to(std::back_inserter(out), FMT_COMPILE("{}.style['{}']='{}';"), var_, cssNames_[p - static_cast<unsigned int>(Property::StylePosition)], i->second);
                    //
                    // out << var_ << ".style['"
                    //     << cssNames_[p - static_cast<unsigned int>(Property::StylePosition)]
                    //     << "']='" << i->second << "';";
                } else {
                    fmt::format_to(std::back_inserter(out), FMT_COMPILE("{}.style.{}='{}';"), var_, cssCamelNames_[p - static_cast<unsigned int>(Property::Style)], i->second);
                    //
                    // out << var_ << ".style."
                    //     << cssCamelNames_[p - static_cast<unsigned int>(Property::Style)]
                    //     << "='" << i->second << "';";
                }
            }
        }
        }

        out.push_back('\n');
        //out << '\n';
}
}

void DomElement::setJavaScriptProperties(EscapeOStream& out,
                                         WApplication *app) const
{
#ifndef WT_TARGET_JAVA
    EscapeOStream escaped(out);
#else
    EscapeOStream escaped = out.push();
#endif // WT_TARGET_JAVA

    bool pushed = false;

    for (auto i = properties_.begin(); i != properties_.end(); ++i) {
        declare(out);

        switch(i->first) {
        case Property::InnerHTML:
        case Property::AddedInnerHTML:
            /*
       * In all cases, setJavaScriptProperties() is followed by
       * renderInnerHtmlJS() which also considers children.
       *
       * When there's 'AddedInnerHTML' then willRenderInnerHtmlJS() should
       * return false, and that's necessary since then we need to pass 'true'
       * as last argument to setHtml()
       */
            if (willRenderInnerHtmlJS(app))
                break;

            out << WT_CLASS ".setHtml(" << var_ << ',';
            if (!pushed) {
                escaped.pushEscape(EscapeOStream::JsStringLiteralSQuote);
                pushed = true;
            }
            fastJsStringLiteral(out, escaped, i->second);
            if (i->first == Property::InnerHTML)
                out << ",false";
            else
                out << ",true";

            out << ");";

            break;
        case Property::Value:
            out << var_ << ".value=";
            if (!pushed) {
                escaped.pushEscape(EscapeOStream::JsStringLiteralSQuote);
                pushed = true;
            }
            fastJsStringLiteral(out, escaped, i->second);
            out << ';';
            break;
        case Property::Target:
            out << var_ << ".target='" << i->second << "';";
            break;
        case Property::Indeterminate:
            out << var_ << ".indeterminate=" << i->second << ";";
            break;
        case Property::Disabled:
            if (type_ == DomElementType::A) {
                if (i->second == "true")
                    out << var_ << ".setAttribute('disabled', 'disabled');";
                else
                    out << var_ << ".removeAttribute('disabled', 'disabled');";
            } else
                out << var_ << ".disabled=" << i->second << ';';
            break;
        case Property::ReadOnly:
            out << var_ << ".readOnly=" << i->second << ';';
            break;
        case Property::TabIndex:
            out << var_ << ".tabIndex=" << i->second << ';';
            break;
        case Property::Checked:
            out << var_ << ".checked=" << i->second << ';';
            break;
        case Property::Selected:
            out << var_ << ".selected=" << i->second << ';';
            break;
        case Property::SelectedIndex:
            out << "setTimeout(function() { "
                << var_ << ".selectedIndex=" << i->second << ";}, 0);";
            break;
        case Property::Multiple:
            out << var_ << ".multiple=" << i->second << ';';
            break;
        case Property::Src:
            out << var_ << ".src='" << i->second << "\';";
            break;
        case Property::ColSpan:
            out << var_ << ".colSpan=" << i->second << ";";
            break;
        case Property::RowSpan:
            out << var_ << ".rowSpan=" << i->second << ";";
            break;
        case Property::Label:
            out << var_ << ".label=";
            if (!pushed) {
                escaped.pushEscape(EscapeOStream::JsStringLiteralSQuote);
                pushed = true;
            }
            fastJsStringLiteral(out, escaped, i->second);
            out << ';';
            break;
        case Property::Placeholder:
            out << var_ << ".placeholder=";
            if (!pushed) {
                escaped.pushEscape(EscapeOStream::JsStringLiteralSQuote);
                pushed = true;
            }
            fastJsStringLiteral(out, escaped, i->second);
            out << ';';
            break;
        case Property::Class:
            out << var_ << ".className=";
            if (!pushed) {
                escaped.pushEscape(EscapeOStream::JsStringLiteralSQuote);
                pushed = true;
            }
            fastJsStringLiteral(out, escaped, i->second);
            out << ';';
            break;
        case Property::StyleFloat:
            out << var_ << ".style.";
            if (app->environment().agentIsIE())
                out << "styleFloat";
            else
                out << "cssFloat";
            out << "=\'" << i->second << "\';";
            break;
        case Wt::Property::StyleWidthExpression:
            out << var_ << ".style.setExpression('width',";
            if (!pushed) {
                escaped.pushEscape(EscapeOStream::JsStringLiteralSQuote);
                pushed = true;
            }
            fastJsStringLiteral(out, escaped, i->second);
            out << ");";
            break;
        default: {
            unsigned int p = static_cast<unsigned int>(i->first);
            if (p >= static_cast<unsigned int>(Property::Style) &&
                p < static_cast<unsigned int>(Property::LastPlusOne)) {
                if (app->environment().agent() == UserAgent::IE6) {
                    /*
                   * Unsupported properties, like min-height, would otherwise be
                   * ignored, but we want this information client-side. (Still, really ?)
                   */
                    out << var_ << ".style['"
                        << cssNames_[p - static_cast<unsigned int>(Property::StylePosition)]
                        << "']='" << i->second << "';";
                } else {
                    out << var_ << ".style."
                        << cssCamelNames_[p - static_cast<unsigned int>(Property::Style)]
                        << "='" << i->second << "';";
                }
            }
        }
        }

        out << '\n';
    }
}
void DomElement::setJavaScriptAttributes(fmt::memory_buffer &out) const
{
    declare(out);
    if(!attributes_.empty())
        fmt::format_to(std::back_inserter(out),
                       FMT_COMPILE("let opt={};Object.keys(opt).forEach(key=>{{if(key==='style'){input}.style.cssText=opt[key];else {input}.setAttribute(key, opt[key]);}});"), fmt::arg("input", var_), attributes_);

    if(!removedAttributes_.empty())
        fmt::format_to(std::back_inserter(out),
                       FMT_COMPILE("{}.forEach(attribute=>{input}.removeAttribute(attribute));"), fmt::arg("input", var_), removedAttributes_);

}


void DomElement::setJavaScriptAttributes(EscapeOStream& out) const
{
    for (AttributeMap::const_iterator i = attributes_.begin();
         i != attributes_.end(); ++i) {
        declare(out);

        if (i->first == "style") {
            out << var_ << ".style.cssText = ";
            jsStringLiteral(out, i->second, '\'');
            out << ';' << '\n';
        } else {
            out << var_ << ".setAttribute('" << i->first << "',";
            jsStringLiteral(out, i->second, '\'');
            out << ");\n";

            //in one line & fmt optimized for the backend
            //auto cc = fmt::format(FMT_COMPILE("let opt={};Object.keys(opt).forEach(key=>{{if(key==='style'){input}.style.cssText=opt[key];else {input}.setAttribute(key, opt[key]);}});"), attributes_, fmt::arg("input", var_));

        }
    }

    for (AttributeSet::const_iterator i = removedAttributes_.begin();
         i != removedAttributes_.end(); ++i) {
        declare(out);

        out << var_ << ".removeAttribute('" << *i << "');\n";

        //auto cc = fmt::format(FMT_COMPILE("{}.forEach(attribute => {input}.removeAttribute(attribute));"), removedAttributes_, fmt::arg("input", var_));
    }
}

bool DomElement::isDefaultInline() const
{
  return isDefaultInline(type_);
}



bool DomElement::isDefaultInline(DomElementType type)
{
  assert(static_cast<unsigned int>(type) < static_cast<unsigned int>(DomElementType::UNKNOWN));
  return defaultInline_[static_cast<unsigned int>(type)];
}

bool DomElement::isSelfClosingTag(const std::string& tag)
{
  return (   (tag == "br")
          || (tag == "hr")
          || (tag == "img")
          || (tag == "area")
          || (tag == "col")
          || (tag == "input")
          || (tag == "link")
          || (tag == "meta"));

}

bool DomElement::isSelfClosingTag(DomElementType element)
{
  return ((   element == DomElementType::BR)
       /* || (element == DomElementType::HR) */
	  || (element == DomElementType::IMG)
	  || (element == DomElementType::AREA)
	  || (element == DomElementType::COL)
	  || (element == DomElementType::INPUT));
}

DomElementType DomElement::parseTagName(const std::string& tag)
{
  for (unsigned i = 0; i < static_cast<unsigned int>(DomElementType::UNKNOWN); ++i)
    if (tag == elementNames_[i])
      return (DomElementType)i;

  return DomElementType::UNKNOWN;
}

std::string DomElement::tagName(DomElementType type)
{
  assert(static_cast<unsigned int>(type) < static_cast<unsigned int>(DomElementType::UNKNOWN));
  return elementNames_[static_cast<unsigned int>(type)];
}

const std::string& DomElement::cssName(Property property)
{
  return cssNames_[static_cast<unsigned int>(property) - 
		   static_cast<unsigned int>(Property::StylePosition)];
}

void DomElement::setGlobalUnfocused(bool b)
{
  globalUnfocused_ = b;
}



}
