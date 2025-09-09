// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#ifndef DOMELEMENT_H_
#define DOMELEMENT_H_

#if defined(WT_THREADED) || defined(WT_TARGET_JAVA)
#include <atomic>
#endif

#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <span>

#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>

#include "Wt/WWebWidget.h"
#include "EscapeOStream.h"


template <typename T>
concept StringLiteral = requires {
    requires std::is_array_v<std::remove_reference_t<T>>;
    requires std::is_same_v<std::remove_extent_t<std::remove_reference_t<T>>, const char>;
};

// Concept for std::string
template <typename T>
concept StdString = requires {
    requires std::is_same_v<std::decay_t<T>, std::string> ||
                 std::is_same_v<std::decay_t<T>, const char*>;
};

template <size_t N>
consteval size_t compute_escaped_size(const char (&s)[N]) {
    size_t size = 0;
    for (size_t i = 0; i < N; ++i) {
        char c = s[i];
        if (c == '\0') break;
        switch (c) {
        case '\\': case '\n': case '\r': case '\t': case '\'':
            size += 2; // Escaped characters take 2 chars (e.g., "\n")
            break;
        default:
            size += 1; // Regular chars take 1 char
            break;
        }
    }
    return size + 1; // Add space for null terminator
}
template <size_t M ,size_t N>
consteval auto escape_js_literal(const char (&s)[N]) {
    //constexpr auto size = compute_escaped_size(s);

    std::array<char, M> result{};
    // Reserve some space if desired (optional)
    // result.reserve(256);
    unsigned it = 0;
    for (std::size_t i = 0; s[i] != '\0'; ++i) {
        char c = s[i];
        switch (c) {
        case '\\': result[++it] = '\\'; result[++it] = '\\'; break;
        case '\n': result[++it] = '\\'; result[++it] = '\n'; break;
        case '\r': result[++it] = '\\'; result[++it] = '\r';  break;
        case '\t': result[++it] = '\\'; result[++it] = '\t';  break;
        case '\'': result[++it] = '\\'; result[++it] = '\'';  break;
        default:   result[++it] =  c; break;
        }
    }
    result[it] = '\0'; // Null-terminate the result
    return result;
}

template <size_t M, size_t N>
consteval auto EscapedAttrib(const char (&literal)[N]) {
    static_assert(std::is_same_v<decltype(literal), const char (&)[N]>,
                  "literal should be a reference to a const char array");

    //constexpr std::string_view sv(literal, N - 1); // Exclude null terminator
    //constexpr size_t Mc = compute_escaped_size(literal);
    //constexpr auto vv = std::string_view(literal);
    //constexpr auto size = compute_escaped_size(literal);

    return escape_js_literal<M>(literal);;
}
#define CONSTEXPR_JS_ESCAPED(str) str //EscapedAttrib<compute_escaped_size(str)>(str)

/* this class is made for attributes and property to ensure that literals are processed at compile time */
struct EscapedString {
    using Escaper = MixedRules<RuleSet::HtmlAttribute>;
    using JsEscaper = MixedRules<RuleSet::JsStringLiteralSQuote>;
    using MixEscaper = MixedRules<RuleSet::HtmlAttribute, RuleSet::JsStringLiteralSQuote>;
    std::string value;
    bool isescaped_;

    // Constructor with std::string& as per your query
    EscapedString(const std::string& val, bool isescaped = false):
        isescaped_(isescaped)
    {
        value = val;
    }

    const std::string& escaped() {
        std::string result;
        if(isescaped_)
            return value;
        else
            MixEscaper::escape(value, result);
        //value.swap(result);
        isescaped_ = true;
        return value;
    }

    // Constructor with constexpr CONSTEXPR_JS_ESCAPED
    template<std::size_t N>
    EscapedString(const std::array<char, N> val) : // value = CONSTEXPR_JS_ESCAPED(val);
        value(val.begin(), val.end()),
        isescaped_(true)
    {
    }

    bool operator==(std::string_view compare) {
        return value == compare;
    }
};

namespace Wt {

class WApplication;

typedef EscapeOStream EStream;

/*! \brief Enumeration for a DOM property.
 *
 * This is an internal API, subject to change.
 */
enum class Property { InnerHTML, AddedInnerHTML,
		Value, Disabled,
		Checked, Selected, SelectedIndex,
		Multiple, Target, Download, Indeterminate,
		Src,
		ColSpan, RowSpan, ReadOnly,
		TabIndex, Label,
		Class,
                Placeholder,
                Style,
		StyleWidthExpression,
		StylePosition,
		StyleZIndex, StyleFloat, StyleClear,
		StyleWidth, StyleHeight,
		StyleLineHeight,
		StyleMinWidth, StyleMinHeight,
		StyleMaxWidth, StyleMaxHeight,
		StyleLeft, StyleRight,
		StyleTop, StyleBottom,
		StyleVerticalAlign, StyleTextAlign,
		StylePadding,
		StylePaddingTop, StylePaddingRight,
		StylePaddingBottom, StylePaddingLeft,
                StyleMargin,
		StyleMarginTop, StyleMarginRight,
		StyleMarginBottom, StyleMarginLeft,
		StyleCursor, 
		StyleBorderTop, StyleBorderRight,
		StyleBorderBottom, StyleBorderLeft,
		StyleBorderColorTop, StyleBorderColorRight,
		StyleBorderColorBottom, StyleBorderColorLeft,
		StyleBorderWidthTop, StyleBorderWidthRight,
		StyleBorderWidthBottom, StyleBorderWidthLeft,
		StyleColor,
		StyleOverflowX,
		StyleOverflowY,
		StyleOpacity,
		StyleFontFamily,
		StyleFontStyle,
		StyleFontVariant,
		StyleFontWeight,
		StyleFontSize,
		StyleBackgroundColor,
		StyleBackgroundImage,
		StyleBackgroundRepeat,
		StyleBackgroundAttachment,
		StyleBackgroundPosition,
		StyleTextDecoration, StyleWhiteSpace,
		StyleTableLayout, StyleBorderSpacing,
		StyleBorderCollapse,
		StylePageBreakBefore, StylePageBreakAfter,
		StyleZoom,
		StyleVisibility, StyleDisplay,

		/* CSS 3 */
		StyleBoxSizing,
		StyleFlex,
		StyleFlexFlow,
		StyleAlignSelf,
		StyleJustifyContent,

		/* Keep as last, e.g. for bitset sizing. Otherwise, unused. */
		LastPlusOne };
using PropPair = std::pair<Property, std::string>;
using propList = std::initializer_list<std::pair<const Wt::Property, std::string>>;
using attribList = std::initializer_list<std::pair<const std::string, std::string>>;

/*! \class DomElement web/DomElement web/DomElement
 *  \brief Class to represent a client-side DOM element (proxy).
 *
 * The DOM element proxy object is used as an intermediate layer to
 * render the creation of new DOM elements or updates to existing DOM
 * elements. A DOM element can be serialized to HTML or to JavaScript
 * manipulations, and therefore is the main abstraction layer to avoid
 * hard-coding JavaScript-based rendering within the library while
 * still allowing fine-grained Ajax updates or large-scale HTML
 * changes.
 *
 * This is an internal API, subject to change.
 */
class WT_API DomElement
{
public:
  /*! \brief Enumeration for the access mode (creation or update) */
  enum class Mode { Create, Update };

#ifndef WT_TARGET_JAVA
  /*! \brief A map for property values */
  //typedef std::unordered_map<Wt::Property, std::string> PropertyMap;
  typedef boost::unordered::unordered_flat_map<Wt::Property, std::string> PropertyMap;
#else
  typedef std::treemap<Wt::Property, std::string> PropertyMap;
#endif
  /*! \brief Constructor.
   *
   * This constructs a DomElement reference, with a given mode and
   * element type. Note that even when updating an existing element,
   * the type is taken into account for information on what kind of
   * operations are allowed (workarounds for IE deficiencies for
   * examples) or to infer some basic CSS defaults for it (whether it
   * is inline or a block element).
   *
   * Typically, elements are created using one of the 'named'
   * constructors: createNew(), getForUpdate() or updateGiven().
   */
  DomElement(Mode mode, DomElementType type);


  DomElement(Mode mode, DomElementType type, std::string id, bool isId = true) : DomElement(mode, type)
  { if(isId) id_ = id; else var_ = id; }

  DomElement(Mode mode, DomElementType type, const std::span<const PropPair> prop, const attribList& attrib = {}, const std::string& id = "");

  DomElement(Mode mode, DomElementType type, DomElement&& child, const std::span<const PropPair> prop = {}, const attribList& attrib = {}, const std::string& id = "") : DomElement(mode, type, prop, attrib, id)
  { addChild(std::forward<DomElement>(child)); }

  DomElement(DomElement&&) noexcept = default;  // Move constructor

  DomElement& operator=(DomElement&&) noexcept = default;  // Move assignment operator
  /*! \brief Destructor.
   */
  ~DomElement();

  /*! \brief set dom element custom tag name 
   */
  void setDomElementTagName(const std::string& name);

  /*! \brief Low-level URL encoding function.
   */
  static std::string urlEncodeS(std::string_view url, const uint8_t charset[]);

  /*! \brief Low-level URL encoding function.
   *
   * This variant allows the exclusion of certain characters from URL
   * encoding.
   */
  // static std::string urlEncodeS(const std::string& url,
  //                               const std::string& allowed);

  // static std::string urlEncodeS(std::string_view url,
  //                               const std::string &allowed);

  /*! \brief Returns the mode.
   */
  Mode mode() const { return mode_; }
  
  /*! \brief Sets the element type.
   */
  void setType(DomElementType type);

  /*! \brief Returns the element type.
   */
  DomElementType type() const { return type_; }

  /*! \brief Creates a reference to a new element.
   */
  static DomElement createNew(DomElementType type);

  /*! \brief Creates a reference to an existing element, using its ID.
   */
  static DomElement getForUpdate(const std::string& id, DomElementType type);

  /*! \brief Creates a reference to an existing element, deriving the ID from
   *         an object.
   *
   * This uses object->id() as the id.
   */
  static DomElement getForUpdate(const WObject *object, DomElementType type);

  /*! \brief Creates a reference to an existing element, using an expression
   *         to access the element.
   */
  static DomElement updateGiven(const std::string& el, DomElementType type);

  /*! \brief Returns the JavaScript variable name.
   *
   * This variable name is only defined when the element is being
   * rendered using JavaScript, after declare() has been called.
   */
  std::string var() { return var_; }

  /*! \brief Sets whether the element was initially empty.
   *
   * Knowing that an element was empty allows optimization of
   * addChild()
   */
  void setWasEmpty(bool how);

  /*! \brief Adds a child.
   *
   * Ownership of the child is transferred to this element, and the
   * child should not be manipulated after the call, since it could be
   * that it gets directly converted into HTML and deleted.
   */
  void addChild(DomElement &&child);

  DomElement& addChild(DomElementType type);

  DomElement& addChild(const std::string &id, DomElementType type);


  //template <typename DomElement>
  // void addChild(DomElement& child) {
  //     addChild(std::move(child));
  // }
  /*! \brief Inserts a child.
   *
   * Ownership of the child is transferred to this element, and the child
   * should not be manipulated after the call.
   */
  void insertChildAt(DomElement &&child, int pos);

  /*! \brief Saves an existing child.
   *
   * This detaches the child from the parent, allowing the
   * manipulation of the innerHTML without deleting the child. Stubs
   * in the the new HTML that reference the same id will be replaced
   * with the saved child.
   */
  void saveChild(const std::string& id);

  /*! \brief Sets an attribute value.
   */
  // void setAttribute(const std::string& attribute, const std::string& value, bool isEscaped = false)
  // {
  //     ++numManipulations_;
  //     attributes_[attribute] = EscapedString(value, isEscaped);
  //     removedAttributes_.erase(attribute);
  // }

  template <typename T>
  requires (StringLiteral<T> || StdString<T>)
  void setAttribute(const std::string& attribute, T&& value, bool isEscaped = false) {
      if constexpr (StringLiteral<T>) { //literal char[]
          ++numManipulations_;
          attributes_.emplace(attribute, EscapedString(/*CONSTEXPR_JS_ESCAPED*/(value)));
          removedAttributes_.erase(attribute);
      } else {
          ++numManipulations_;
          attributes_.emplace(attribute, EscapedString(value, isEscaped));
          removedAttributes_.erase(attribute);
      }
  }
  // template<std::size_t N>
  // void setAttribute(const std::string& attribute, const char (&value)[N]) {
  //     ++numManipulations_;
  //     attributes_.emplace(attribute, EscapedString(CONSTEXPR_JS_ESCAPED(value)));
  //     removedAttributes_.erase(attribute);
  // }


  // template <StringLiteral T>
  // void setAttribute(const std::string& attribute, T&& value, bool isEscaped = false) {
  //     ++numManipulations_;
  //     attributes_.emplace(attribute, EscapedString(CONSTEXPR_JS_ESCAPED(std::forward<T>(value))));
  //     removedAttributes_.erase(attribute);
  // }

  // Overload for std::string
  // template <StdString T>
  // void setAttribute(const std::string& attribute, T&& value, bool isEscaped = false) {
  //     ++numManipulations_;
  //     attributes_.emplace(attribute, EscapedString(std::forward<T>(value), isEscaped));
  //     removedAttributes_.erase(attribute);
  // }

  void setAttribute(const std::string& attribute, EscapedString&& value)
  {
      ++numManipulations_;
      attributes_.emplace(attribute, std::move(value));
      removedAttributes_.erase(attribute);
  }
  template <typename T>
  requires (StringLiteral<T> || StdString<T>)
  void tryEmplaceAttribute(const std::string& attribute, T&& value)
  {
      ++numManipulations_;
      if(attributes_.try_emplace(attribute, EscapedString(/*CONSTEXPR_JS_ESCAPED*/(value))).second)
          removedAttributes_.erase(attribute);
  }

  template<typename T>
  requires std::is_arithmetic_v<T>
  void setAttribute(const std::string& attribute, T value) {
      // if constexpr (std::integral<T>) {
      // }
      // else {
      //     setAttribute(attribute, fmt::format("{:.2}", value), true);
      // }
      setAttribute(attribute, fmt::format(FMT_COMPILE("{}"), std::forward<T>(value)), true);
  }

  /*! \brief Returns an attribute value set.
   *
   * \sa setAttribute()
   */
  std::string getAttribute(const std::string& attribute) const;

  /*! \brief Removes an attribute.
   */
  void removeAttribute(const std::string& attribute);

  /*! \brief Sets a property.
   */
  void setProperty(Wt::Property property, std::string_view value)
  {
      ++numManipulations_;
      properties_[property] = value;

      if (property >= Property::StyleMinWidth && property <= Property::StyleMaxHeight)
          minMaxSizeProperties_ = true;


      switch (property) {
      case Property::Style:

          break;
      default:
          break;
      }

      if(property >= Property::Style)
          hasCssRules_ = true;

      // if (static_cast<unsigned int>(property) >= static_cast<unsigned int>(Property::Style))
      //       hasCssRules_ = true;

  }

  /*! \brief Adds a 'word' to a property.
   *
   * This adds a word (delimited by a space) to an existing property value.
   */
  void addPropertyWord(Wt::Property property, const std::string& value);

  /*! \brief Returns a property value set.
   *
   * \sa setProperty()
   */
  std::string getProperty(Wt::Property property) const;

  /*! \brief Removes a property.
   */
  void removeProperty(Wt::Property property);

  /*! \brief Sets a whole map of properties.
   */
  void setProperties(const PropertyMap& properties);

  /*! \brief Returns all properties currently set.
   */
  const PropertyMap& properties() const { return properties_; }

  /*! \brief Clears all properties.
   */
  void clearProperties();

  /*! \brief Sets an event handler based on a signal's connections.
   */
  void setEventSignal(const char *eventName, const EventSignalBase& signal);

  /*! \brief Sets an event handler.
   *
   * This sets an event handler by a combination of client-side
   * JavaScript code and a server-side signal to emit.
   */
  void setEvent(const char *eventName,
		const std::string& jsCode,
		const std::string& signalName,
		bool isExposed = false);

  /*! \brief Sets an event handler.
   *
   * This sets a JavaScript event handler.
   */
  void setEvent(const char *eventName, const std::string& jsCode);

  /*! \brief This adds more JavaScript to an event handler.
   */
  void addEvent(const char *eventName, const std::string& jsCode);

  /*! \brief A data-structure for an aggregated event handler. */ 
  struct EventAction
  {
    std::string jsCondition;
    std::string jsCode;
    std::string updateCmd;
    bool        exposed;

    EventAction(const std::string& jsCondition, const std::string& jsCode,
		const std::string& updateCmd, bool exposed);
  };

  /*! \brief Sets an aggregated event handler. */
  void setEvent(const char * eventName,
		const std::vector<EventAction>& actions);

  /*! \brief Sets the DOM element id.
   */
  void setId(const std::string& id);

  /*! \brief Sets a DOM element name.
   */
  void setName(const std::string& name);

  /*! \brief Configures the DOM element as a source for timed events.
   */
  void setTimeout(int msec, bool jsRepeat);

  /*! \brief Configures the DOM element as a source for timed events,
   *         with given initial delay and interval, always repeating.
   */
  void setTimeout(int delay, int interval);

  /*! \brief Calls a JavaScript method on the DOM element.
   */
  //void callMethod(const std::string& method);

  void callMethod(std::string_view method);

  // template <typename... Args>
  // FMT_INLINE void callMethod(fmt::format_string<typename fmtlogdetail::UnrefPtr<fmt::remove_cvref_t<Args>>::type...> method, Args&&... args) {
  //     ++numManipulations_;

  //     if (var_.empty())
  //         fmt::format_to(std::back_inserter(javaScript_), "{}.$('{}').", WT_CLASS, id_);
  //     else
  //         fmt::format_to(std::back_inserter(javaScript_), "{}.", var_);

  //     fmt::vformat_to(std::back_inserter(javaScript_), method, fmt::make_format_args(FMT_FORWARD(args)...));
  //     fmt::format_to(std::back_inserter(javaScript_), ";\n");
  // }
  template<typename... Args>
  void callMethod(
      fmt::format_string<Args...> method,
      Args&&... args
      ) {
      ++numManipulations_;

      if (var_.empty())
          fmt::format_to(std::back_inserter(javaScript_), "{}.$('{}').", WT_CLASS, id_);
      else
          fmt::format_to(std::back_inserter(javaScript_), "{}.", var_);

      fmt::vformat_to(std::back_inserter(javaScript_), method, fmt::make_format_args(FMT_FORWARD(args)...));
      fmt::format_to(std::back_inserter(javaScript_), ";\n");
  }
  /* compile string template */
  template<typename Format, typename... Args>
  requires(std::is_base_of_v<fmt::detail::compiled_string, std::remove_cv_t<Format>>)
  void callMethod(
      Format&& method,
      Args&&... args
      ) {
      ++numManipulations_;

      if (var_.empty())
          fmt::format_to(std::back_inserter(javaScript_), "{}.$('{}').", WT_CLASS, id_);
      else
          fmt::format_to(std::back_inserter(javaScript_), "{}.", var_);

      fmt::format_to(std::back_inserter(javaScript_), std::forward<Format>(method), std::forward<Args>(args)...);
      fmt::format_to(std::back_inserter(javaScript_), ";\n");
  }

  /*! \brief Calls JavaScript (related to the DOM element).
   */
  void callJavaScript(const std::string& javascript, bool evenWhenDeleted = false);

  // template<bool evenWhenDeleted = false, typename... Args>
  // void callJavaScript(fmt::format_string<typename fmtlogdetail::UnrefPtr<fmt::remove_cvref_t<Args>>::type...> format, Args&&... args) {
  //     ++numManipulations_;
  //     if constexpr (!evenWhenDeleted) {
  //         fmt::vformat_to(std::back_inserter(javaScript_), format, fmt::make_format_args(FMT_FORWARD(args)...));
  //         fmt::format_to(std::back_inserter(javaScript_), "\n");
  //     }
  //     else
  //         fmt::vformat_to(std::back_inserter(javaScriptEvenWhenDeleted_), format, fmt::make_format_args(FMT_FORWARD(args)...));
  // }
  /* runtime string template */
  template<bool evenWhenDeleted = true, typename... Args>
  void callJavaScript(
      fmt::format_string<Args...> format,
      Args&&... args
      ) {
      ++numManipulations_;
      if constexpr (!evenWhenDeleted) {
          fmt::vformat_to(std::back_inserter(javaScript_), format, fmt::make_format_args(FMT_FORWARD(args)...));
          fmt::format_to(std::back_inserter(javaScript_), "\n");
      }
      else
          fmt::vformat_to(std::back_inserter(javaScriptEvenWhenDeleted_), format, fmt::make_format_args(FMT_FORWARD(args)...));
  }
  /* compile string template */
  template<bool evenWhenDeleted = true, typename Format, typename... Args>
  requires(std::is_base_of_v<fmt::detail::compiled_string, std::remove_cv_t<Format>>)
  void callJavaScript(
      Format&& format,
      Args&&... args
      ) {
      ++numManipulations_;
      if constexpr (!evenWhenDeleted) {
          fmt::format_to(std::back_inserter(javaScript_), std::forward<Format>(format), std::forward<Args>(args)...);
          fmt::format_to(std::back_inserter(javaScript_), "\n");
      }
      else
          fmt::format_to(std::back_inserter(javaScriptEvenWhenDeleted_), std::forward<Format>(format), std::forward<Args>(args)...);
  }

  /*! \brief Returns the id.
   */
  const std::string& id() const { return id_; }

  /*! \brief Removes all children.
   *
   * If firstChild != 0, then only children starting from firstChild
   * are removed.
   */
  void removeAllChildren(int firstChild = 0);

  /*! \brief Removes the element.
   */
  void removeFromParent();

  /*! \brief Replaces the element by another element.
   */
  //template<typename DomElement>
  void replaceWith(DomElement&& newElement);

  //void replaceWith(std::unique_ptr<DomElement> newElement);

  /*! \brief Unstubs an element by another element.
   *
   * Stubs are used to render hidden elements initially and update
   * them in the background. This is almost the same as replaceWith()
   * except that some style properties are copied over (most
   * importantly its visibility).
   */
  void unstubWith(DomElement&& newElement, bool hideWithDisplay);

  /*! \brief Inserts the element in the DOM as a new sibling.
   */
  void insertBefore(DomElement *sibling);

  /*! \brief Unwraps an element to progress to Ajax support.
   *
   * In plain HTML mode, some elements are rendered wrapped in or as
   * another element, to provide more interactivity in the absense of
   * JavaScript.
   */
  //void unwrap();

  /*! \brief Enumeration for an update rendering phase.
   */
  enum class Priority { Delete, Create, Update };

  /*! \brief Structure for keeping track of timers attached to this element.
   */
  struct TimeoutEvent {
    int msec;
    std::string event;
    int repeat;

    TimeoutEvent() { }
    TimeoutEvent(int m, const std::string& e, int r)
      : msec(m), event(e), repeat(r) { }
  };

  /*! \brief A list of timeouts.
   */
  typedef std::vector<TimeoutEvent> TimeoutList;

  /*! \brief Renders the element as JavaScript.
   */
  void asJavaScript(WStringStream& out);
  void asJavaScript(fmt::memory_buffer& out);


  /*! \brief Renders the element as JavaScript, by phase.
   *
   * To avoid temporarily having dupliate IDs as elements move around
   * in the page, rendering is ordered in a number of phases : first
   * deleting existing elements, then creating new elements, and
   * finally updates to existing elements.
   */
  std::string asJavaScript(EStream& out, Priority priority) const;
  std::string asJavaScript(fmt::memory_buffer& out, Priority priority) const;


  /*! \brief Renders the element as HTML.
   *
   * Anything that cannot be rendered as HTML is rendered as
   * javaScript as a by-product.
   */
  void asHTML(EStream& out, EStream& javaScript, TimeoutList& timeouts, bool openingTagOnly = false) const;

  void asHTML(fmt::memory_buffer& out, fmt::memory_buffer& javaScript, TimeoutList& timeouts, bool openingTagOnly = false) const;

   /*! \brief Creates the JavaScript statements for timer rendering.
   */
  static void createTimeoutJs(WStringStream& out, const TimeoutList& timeouts, WApplication *app);
  static void createTimeoutJs(fmt::memory_buffer& out, const TimeoutList& timeouts, WApplication *app);

  /*! \brief Returns the default display property for this element.
   *
   * This returns whether the element is by default an inline or block
   * element.
   */
  bool isDefaultInline() const;

  /*! \brief Declares the element.
   *
   * Only after the element has been declared, var() returns a useful
   * JavaScript reference.
   */
  void declare(EStream& out) const;

  void declare(fmt::memory_buffer& out) const;

  /*! \brief Renders properties and attributes into CSS.
   */
  std::string cssStyle() const;

  void cssStyle(fmt::memory_buffer& out) const;

  /*! \brief Utility for rapid rendering of JavaScript strings.
   *
   * It uses pre-computed mixing rules for escaping of the string.
   */
  static void fastJsStringLiteral(EStream& outRaw,
				  const EStream& outEscaped,
				  const std::string& s);

  /*! \brief Utility that renders a string as JavaScript literal.
   */
  static void jsStringLiteral(EStream& out, const std::string& s,
			      char delimiter);

  /*! \brief Utility that renders a string as JavaScript literal.
   */
  static void jsStringLiteral(WStringStream& out, const std::string& s,
			      char delimiter);

  /*! \brief Utility for rapid rendering of HTML attribute values.
   *
   * It uses pre-computed mixing rules for escaping of the attribute
   * value.
   */
  static void fastHtmlAttributeValue(EStream& outRaw,
                                     const EStream& outEscaped,
                                     const std::string& s);

  /*! \brief Utility that renders a string as HTML attribute.
   */
  static void htmlAttributeValue(WStringStream& out, const std::string& s);

  /*! \brief Returns whether a tag is self-closing in HTML.
   */
  static bool isSelfClosingTag(const std::string& tag);

  /*! \brief Returns whether a tag is self-closing in HTML.
   */
  static bool isSelfClosingTag(DomElementType element);

  /*! \brief Parses a tag name to a DOMElement type.
   */
  static DomElementType parseTagName(const std::string& tag);

  /*! \brief Returns the tag name for a DOMElement type.
   */
  static std::string tagName(DomElementType type);

  /*! \brief Returns the name for a CSS property, as a string.
   */
  static const std::string& cssName(Property property);

  /*! \brief Returns whether a paritcular element is by default inline.
   */
  static bool isDefaultInline(DomElementType type);

  /*! \brief Returns all custom JavaScript collected in this element.
   */
  std::string_view javaScript() const { return std::string_view(javaScript_); }

  /*! \brief Something to do with broken IE Mobile 5 browsers...
   */
  void updateInnerHtmlOnly();

  /*! \brief Adds an element to a parent, using suitable methods.
   *
   * Depending on the type, different DOM methods are needed. In
   * particular for table cells, some browsers require dedicated API
   * instead of generic insertAt() or appendChild() functions.
   */
  std::string addToParent(WStringStream& out, const std::string& parentVar, int pos, WApplication *app);
  std::string addToParent(fmt::memory_buffer& out, const std::string& parentVar, int pos, WApplication *app);


  /*! \brief Renders the element as JavaScript, and inserts it in the DOM.
   */
  void createElement(WStringStream& out, WApplication *app, const std::string& domInsertJS);

  /*! \brief Allocates a JavaScript variable.
   */
  std::string& createVar() const;

  void setGlobalUnfocused(bool b);

private:
  struct EventHandler {
    std::string jsCode;
    std::string signalName;

    EventHandler() { }
    EventHandler(const std::string& j, const std::string& sn)
      : jsCode(j), signalName(sn) { }
  };

  typedef boost::unordered_flat_map<std::string, EscapedString> AttributeMap;
  typedef boost::unordered_flat_set<std::string> AttributeSet;
  //typedef boost::unordered_flat_map<const char *, EventHandler> EventHandlerMap;

  //typedef std::unordered_map<std::string, EscapedString> AttributeMap;
  //typedef std::unordered_set<std::string> AttributeSet;
  typedef std::unordered_map<const char *, EventHandler> EventHandlerMap;

  bool willRenderInnerHtmlJS(WApplication *app) const;
  bool canWriteInnerHTML(WApplication *app) const;
  bool containsElement(DomElementType type) const;
  void processEvents(WApplication *app) const;
  void processProperties(WApplication *app) const;
  void setJavaScriptProperties(EStream& out, WApplication *app) const;
  void setJavaScriptProperties(fmt::memory_buffer& out, WApplication *app) const;

  void setJavaScriptAttributes(EStream& out) const;
  void setJavaScriptAttributes(fmt::memory_buffer& out) const;
  void setJavaScriptEvent(EStream& out, const char *eventName,
			  const EventHandler& handler, WApplication *app) const;
  void setJavaScriptEvent(fmt::memory_buffer& out, const char *eventName,
                          const EventHandler& handler, WApplication *app) const;
  void createElement(EStream& out, WApplication *app, const std::string& domInsertJS);
  void createElement(fmt::memory_buffer& out, WApplication *app, std::string_view domInsertJS);

  std::string addToParent(EStream& out, const std::string& parentVar, int pos, WApplication *app);
  std::string createAsJavaScript(EStream& out,
                                 const std::string& parentVar, int pos,
                                 WApplication *app);
  void renderInnerHtmlJS(EStream& out, WApplication *app) const;
  void renderDeferredJavaScript(EStream& out) const;

  void renderInnerHtmlJS(fmt::memory_buffer& out, WApplication *app) const;
  void renderDeferredJavaScript(fmt::memory_buffer& out) const;

  Mode         mode_;
  bool         wasEmpty_;
  bool	       hasCssRules_ = false;
  mutable bool	       needButtonWrap_;
  int	       removeAllChildren_;
  bool         hideWithDisplay_;
  bool         minMaxSizeProperties_;
  bool         unstubbed_;
  bool         unwrapped_;
  std::unique_ptr<DomElement> replaced_;        // when replaceWith() is called
  DomElement  *insertBefore_;
  DomElementType type_;
  std::string  id_;
  mutable std::string innerHTML_; //for fmt
  int          numManipulations_;
  int          timeOut_;
  int          timeOutJSRepeat_;
  fmt::memory_buffer    javaScript_;
  EStream      javaScript3_;
  std::string  javaScriptEvenWhenDeleted_;
  mutable std::string var_;
  mutable bool declared_;
  bool globalUnfocused_;

  AttributeMap    attributes_;
  AttributeSet    removedAttributes_;
  PropertyMap     properties_;
  EventHandlerMap eventHandlers_;

  // struct ChildInsertion {
  //   int pos;
  //   DomElement child;

  //   ChildInsertion() : pos(0), child(nullptr) { }
  //   ChildInsertion(int p, DomElement &&c) : pos(p), child(std::move(c)) { }
  // };
  struct ChildInsertion;  // Forward declaration

  std::vector<ChildInsertion> childrenToAdd_;
  std::vector<std::string> childrenToSave_;
  std::vector<DomElement> updatedChildren_;
  fmt::memory_buffer childrenHtml_;
  EStream childrenHtml2_;
  TimeoutList timeouts_;
  std::string elementTagName_;

#if defined(WT_THREADED) || defined(WT_TARGET_JAVA)
  static std::atomic<unsigned> nextId_;
#else
  static unsigned nextId_;
#endif

  friend class WCssDecorationStyle;
  friend struct fmt::formatter<Wt::DomElement>;
  friend struct fmt::formatter<const Wt::DomElement>;
  friend struct fmt::formatter<std::tuple<const std::vector<Wt::DomElement::ChildInsertion>&, fmt::memory_buffer&, fmt::memory_buffer&, std::vector<Wt::DomElement::TimeoutEvent>&>>;
  friend struct fmt::formatter<std::tuple<const Wt::DomElement&, fmt::memory_buffer&, fmt::memory_buffer&, std::vector<Wt::DomElement::TimeoutEvent>&>>;
};

struct DomElement::ChildInsertion {
    int pos = 0;
    DomElement child;

    //ChildInsertion() : pos(0), child() { }
    ChildInsertion(int p, DomElement &&c) : pos(p), child(std::move(c)) { }
};

} // namespace Wt

struct JsString {
    std::string_view value;
    explicit JsString(std::string_view sv) : value(sv) {}
};

// Formatter specialization for JsStringView with special tags
template <>
struct fmt::formatter<JsString> {
    char jstype = 0; // Default to double quote escaping
    char htmltype = 0;
    RuleSet jsrule, htmlrule;
    // Parse format specifiers (e.g., {d} for double quote, {s} for single quote)
    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && (*it == 'd' || *it == 's')) {
            if(*it == 'd')
                jsrule = RuleSet::JsStringLiteralDQuote;
            else if(*it == 's')
                jsrule = RuleSet::JsStringLiteralSQuote;
            jstype = *it++;
        }
        if(it != end && (*it == 'h' || *it == 'p' || *it == 'n')) {
            if(*it == 'h')
                htmlrule = RuleSet::HtmlAttribute;
            else if(*it == 'p')
                htmlrule = RuleSet::PlainText;
            else if(*it == 'n')
                htmlrule = RuleSet::PlainTextNewLines;
            htmltype = *it++;
        }
        if (it != end && *it != '}') {
            throw format_error("invalid format specifier");
        }
        return it;
    }

    // Format based on specifier
    template <typename FormatContext>
    auto format(const JsString& jsv, FormatContext& ctx) const {
        std::string escaped;
        auto out = ctx.out();

        if(jstype && !htmltype) {
            if(jstype == 's') {
                using Escaper = MixedRules<RuleSet::JsStringLiteralSQuote>;
                Escaper::escape(jsv.value, escaped);
            }
            else {
                using Escaper = MixedRules<RuleSet::JsStringLiteralDQuote>;
                Escaper::escape(jsv.value, escaped);
            }
        }
        else if(htmltype && !jstype) {
            if(htmltype == 'h') {
                using Escaper = MixedRules<RuleSet::HtmlAttribute>;
                Escaper::escape(jsv.value, escaped);
            }
            else if(htmltype == 'p') {
                using Escaper = MixedRules<RuleSet::PlainText>;
                Escaper::escape(jsv.value, escaped);
            }
            else {
                using Escaper = MixedRules<RuleSet::PlainTextNewLines>;
                Escaper::escape(jsv.value, escaped);
            }
        }
        else if(htmltype && jstype) {
            if(htmltype == 'h') {
                if(jstype == 's') {
                    using Escaper = MixedRules<RuleSet::HtmlAttribute, RuleSet::JsStringLiteralSQuote>;
                    Escaper::escape(jsv.value, escaped);
                }
                else {
                    using Escaper = MixedRules<RuleSet::HtmlAttribute, RuleSet::JsStringLiteralDQuote>;
                    Escaper::escape(jsv.value, escaped);
                }
            }
            else if(htmltype == 'p') {
                if(jstype == 's') {
                    using Escaper = MixedRules<RuleSet::PlainText, RuleSet::JsStringLiteralSQuote>;
                    Escaper::escape(jsv.value, escaped);
                }
                else {
                    using Escaper = MixedRules<RuleSet::PlainText, RuleSet::JsStringLiteralDQuote>;
                    Escaper::escape(jsv.value, escaped);
                }
            }
            else {
                if(jstype == 's') {
                    using Escaper = MixedRules<RuleSet::PlainTextNewLines, RuleSet::JsStringLiteralSQuote>;
                    Escaper::escape(jsv.value, escaped);
                }
                else {
                    using Escaper = MixedRules<RuleSet::PlainTextNewLines, RuleSet::JsStringLiteralDQuote>;
                    Escaper::escape(jsv.value, escaped);
                }
            }
        }
        return fmt::format_to(out, FMT_COMPILE("{}"), jsv.value);
        //if no parameter
        //return fmt::format_to(out, FMT_COMPILE("{}"), jsv.value);
    }
};
#endif // DOMELEMENT_H_
