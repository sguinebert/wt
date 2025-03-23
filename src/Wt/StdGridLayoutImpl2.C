/*
 * Copyright (C) 2012 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include "Wt/WApplication.h"
#include "Wt/WContainerWidget.h"
#include "Wt/WEnvironment.h"
#include "Wt/WGridLayout.h"
#include "Wt/WLogger.h"

#include "StdGridLayoutImpl2.h"
#include "SizeHandle.h"
#include "DomElement.h"
#include "WebUtils.h"

#ifndef WT_DEBUG_JS
#include "js/StdGridLayoutImpl2.min.js"
#include "js/WtResize.min.js"
#endif

#ifdef WT_WIN32
#define snprintf _snprintf
#endif
namespace fmt {

// Specialize formatter for std::vector<Impl::Grid::Section>
template <>
struct formatter<Wt::StdGridLayoutImpl2*> {
    char type = 'r'; // Default to 'r' if no specifier is given

    // Parse method to handle 'c' or 'r'
    constexpr auto parse(fmt::format_parse_context& ctx) {
        auto it = ctx.begin(); // Start of the format specifier
        auto end = ctx.end();  // End of the format specifier

        // Check if there's a specifier and it's 'c' or 'r'
        if (it != end && (*it == 'c' || *it == 'r')) {
            type = *it; // Store the specifier ('c' or 'r')
            ++it;       // Move past the specifier
        } else if (it != end && *it != '}') {
            // If there's something but it's not 'c', 'r', or '}', it’s invalid
            throw fmt::format_error("invalid format specifier; expected 'c' or 'r'");
        }
        return it; // Return the iterator position after parsing
    }
    // Format method
    template <typename FormatContext>
    auto format(const Wt::StdGridLayoutImpl2* grid, FormatContext& ctx) const {
        const auto& sections = type == 'r' ? grid->grid_.rows_ : grid->grid_.columns_;
        auto out = ctx.out();

        // Retrieve the WApplication instance at runtime
        WApplication* app = WApplication::instance();

        out = fmt::format_to(out, "[");
        for(unsigned i = 0; i < sections.size(); ++i) {
            auto& section = sections[i];
            auto minsize = type == 'r' ? grid->minimumHeightForRow(i) : grid->minimumWidthForColumn(i);

            if(section.resizable_) {
                SizeHandle::loadJavaScript(app);

                const WLength& size = section.initialSize_;
                if(size.unit() == LengthUnit::Percentage)
                    out = fmt::format_to(out, "[{},[{},1],{}]", section.stretch_, size.value(), minsize);
                else
                    out = fmt::format_to(out, "[{},[{}],{}]", section.stretch_, size.isAuto() ? -1 : size.toPixels(), minsize);
            }
            else
                out = fmt::format_to(out, "[{},0,{}]", section.stretch_, minsize);

            if(i != sections.size() - 1)
                out = fmt::format_to(out, ',');
        }
        out = fmt::format_to(out, "[");
        return out;
    }

};
// Specialize formatter for std::vector<Impl::Grid::Section>
template <>
struct formatter<Wt::Impl::Grid> {
    char type = 'r'; // Default to 'r' if no specifier is given

    // Parse method to handle 'c' or 'r'
    constexpr auto parse(fmt::format_parse_context& ctx) {
        auto it = ctx.begin(); // Start of the format specifier
        auto end = ctx.end();  // End of the format specifier

        // Check if there's a specifier and it's 'c' or 'r'
        if (it != end && (*it == 'c' || *it == 'r')) {
            type = *it; // Store the specifier ('c' or 'r')
            ++it;       // Move past the specifier
        } else if (it != end && *it != '}') {
            // If there's something but it's not 'c', 'r', or '}', it’s invalid
            throw fmt::format_error("invalid format specifier; expected 'c' or 'r'");
        }
        return it; // Return the iterator position after parsing
    }
    // Format method
    template <typename FormatContext>
    auto format(const Wt::Impl::Grid& grid, FormatContext& ctx) const {
        auto out = ctx.out();

        const unsigned colCount = grid.columns_.size();
        const unsigned rowCount = grid.rows_.size();

        for (unsigned row = 0; row < rowCount; ++row) {
            for (unsigned col = 0; col < colCount; ++col) {
                Impl::Grid::Item& item = (Impl::Grid::Item&)grid.items_[row][col];

                AlignmentFlag hAlign = item.alignment_ & AlignHorizontalMask;
                AlignmentFlag vAlign = item.alignment_ & AlignVerticalMask;

                if (row + col != 0)
                    out = fmt::format_to(out, ","); //js << ",";

                if (item.item_) {
                    auto stdlayout = (StdLayoutItemImpl *)(item.item_.get()->impl());
                    std::string id = stdlayout->id();

                    //js << "{";
                    out = fmt::format_to(out, "{{");

                    if (item.colSpan_ != 1 || item.rowSpan_ != 1)
                        out = fmt::format_to(out, "span: [{},{}],", item.colSpan_, item.rowSpan_);//js << "span: [" << item.colSpan_ << "," << item.rowSpan_ << "],";

                    if (item.alignment_.value()) {
                        unsigned align = 0;

                        if (hAlign != static_cast<AlignmentFlag>(0))
                            switch (hAlign) {
                            case AlignmentFlag::Left: align |= 0x1; break;
                            case AlignmentFlag::Right: align |= 0x2; break;
                            case AlignmentFlag::Center: align |= 0x4; break;
                            default: break;
                            }

                        if (vAlign != static_cast<AlignmentFlag>(0))
                            switch (vAlign) {
                            case AlignmentFlag::Top: align |= 0x10; break;
                            case AlignmentFlag::Bottom: align |= 0x20; break;
                            case AlignmentFlag::Middle: align |= 0x40; break;
                            default: break;
                            }

                        //js << "align:" << (int)align << ",";
                        out = fmt::format_to(out, "align:{},", align);
                    }

                    out = fmt::format_to(out, "dirty:{}id:'{}'}}", item.update_ ? 2 : 0, id);

                    // js << "dirty:" << (grid_.items_[row][col].update_ ? 2 : 0)
                    //    << ",id:'" << id << "'"
                    //    << "}";

                    item.update_ = 0;
                } else
                    out = fmt::format_to(out, "null");
            }
        }

        return out;
    }
};

} // namespace fmt
namespace Wt {

LOGGER("WGridLayout2");

StdGridLayoutImpl2::StdGridLayoutImpl2(WLayout *layout, Impl::Grid& grid)
  : StdLayoutImpl(layout, StdGridLayout2),
    grid_(grid),
    needAdjust_(false),
    needRemeasure_(false),
    needConfigUpdate_(false)
{
  const char *THIS_JS = "js/StdGridLayoutImpl2.js";

  WApplication *app = WApplication::instance();

  if (!app->javaScriptLoaded(THIS_JS)) {
    app->styleSheet().addRule("table.Wt-hcenter", "margin: 0px auto;"
			      "position: relative");

    LOAD_JAVASCRIPT(app, THIS_JS, "StdLayout2", wtjs1);
    LOAD_JAVASCRIPT(app, THIS_JS, "layouts2", appjs1);

    app->doJavaScript(app->javaScriptClass() + ".layouts2.scheduleAdjust();");
    app->doJavaScript("(function(){"
                      "var f=function(){"
                      + app->javaScriptClass() + ".layouts2.scheduleAdjust();"
                                                 "};"
                                                 "window.addEventListener('load',f);"
                                                 "})();");

    WApplication::instance()->addAutoJavaScript
      ("if(" + app->javaScriptClass() + ".layouts2) "
       + app->javaScriptClass() + ".layouts2.adjustNow();");
  }
}

bool StdGridLayoutImpl2::itemResized(WLayoutItem *item)
{
  const unsigned colCount = grid_.columns_.size();
  const unsigned rowCount = grid_.rows_.size();

  for (unsigned row = 0; row < rowCount; ++row)
    for (unsigned col = 0; col < colCount; ++col)
      if (grid_.items_[row][col].item_.get() == item &&
	  !grid_.items_[row][col].update_) {
	grid_.items_[row][col].update_ = true;
	needAdjust_ = true;
	return true;
      }

  return false;
}

bool StdGridLayoutImpl2::parentResized()
{
  if (!needRemeasure_) {
    needRemeasure_ = true;
    return true;
  } else
    return false;
}

int StdGridLayoutImpl2::nextRowWithItem(int row, int c) const
{
  for (row += grid_.items_[row][c].rowSpan_; row < (int)grid_.rows_.size();
       ++row) {
    for (unsigned col = 0; col < grid_.columns_.size();
	 col += grid_.items_[row][col].colSpan_) {
      if (hasItem(row, col))
	return row;
    }
  }

  return grid_.rows_.size();
}

int StdGridLayoutImpl2::nextColumnWithItem(int row, int col) const
{
  for (;;) {
    col = col + grid_.items_[row][col].colSpan_;

    if (col < (int)grid_.columns_.size()) {
      for (unsigned i = 0; i < grid_.rows_.size(); ++i)
	if (hasItem(i, col))
	  return col;
    } else
      return grid_.columns_.size();
  }
}

bool StdGridLayoutImpl2::hasItem(int row, int col) const
{
  WLayoutItem *item = grid_.items_[row][col].item_.get();

  if (item) {
    WWidget *w = item->widget();
    return !w || !w->isHidden();
  } else
    return false;
}

DomElement StdGridLayoutImpl2::createElement(WLayoutItem *item,
					      WApplication *app)
{
  DomElement c = getImpl(item)->createDomElement(nullptr, true, true, app);

  c.setProperty(Property::StyleVisibility, "hidden");

  return c;
}

void StdGridLayoutImpl2::updateDom(DomElement& parent)
{
  WApplication *app = WApplication::instance();

  if (needConfigUpdate_) {
    needConfigUpdate_ = false;

    DomElement div = DomElement::getForUpdate(this, DomElementType::DIV);

    for (unsigned i = 0; i < addedItems_.size(); ++i) {
      WLayoutItem *item = addedItems_[i];
      DomElement c = createElement(item, app);
      div.addChild(c);
    }

    addedItems_.clear();

    for (unsigned i = 0; i < removedItems_.size(); ++i)
      parent.callJavaScript(WT_CLASS ".remove('" + removedItems_[i] + "');", true);

    removedItems_.clear();

    parent.addChild(div);

    // WStringStream js;
    // js << app->javaScriptClass() << ".layouts2.updateConfig('"
    //    << id() << "',";
    // streamConfig(js, app);
    // js << ");";

    fmt::memory_buffer js;
    fmt::format_to(std::back_inserter(js), "{}.layouts2.updateConfig('{}',", app->javaScriptClass(), id());
    streamConfig(js, app);
    fmt::format_to(std::back_inserter(js), ");");

    app->doJavaScript(fmt::to_string(js));

    needRemeasure_ = false;
    needAdjust_ = false;
  }

  if (needRemeasure_) {
      needRemeasure_ = false;
      WStringStream js;
      js << app->javaScriptClass() << ".layouts2.setDirty('" << id() << "');";
      app->doJavaScript(js.str());
  }

  if (needAdjust_) {
      needAdjust_ = false;

      WStringStream js;
      js << app->javaScriptClass() << ".layouts2.adjust('" << id() << "', [";

      bool first = true;

      const unsigned colCount = grid_.columns_.size();
      const unsigned rowCount = grid_.rows_.size();

      for (unsigned row = 0; row < rowCount; ++row)
          for (unsigned col = 0; col < colCount; ++col)
              if (grid_.items_[row][col].update_) {
                  grid_.items_[row][col].update_ = false;
                  if (!first)
                      js << ",";
                  first = false;
                  js << "[" << (int)row << "," << (int)col << "]";
              }

      js << "]);";

      app->doJavaScript(js.str());
  }

  const unsigned colCount = grid_.columns_.size();
  const unsigned rowCount = grid_.rows_.size();

  for (unsigned i = 0; i < rowCount; ++i) {
      for (unsigned j = 0; j < colCount; ++j) {
          WLayoutItem *item = grid_.items_[i][j].item_.get();
          if (item) {
              WLayout *nested = item->layout();
              if (nested)
                  (dynamic_cast<StdLayoutImpl *>(nested->impl()))->updateDom(parent);
          }
      }
  }
}

StdGridLayoutImpl2::~StdGridLayoutImpl2()
{ 
  WApplication *app = WApplication::instance();

  /*
   * If it is a top-level layout (as opposed to a nested layout),
   * configure overflow of the container.
   */
  if (parentLayoutImpl() == nullptr) {
    if (container() == app->root()) {
      app->setBodyClass("");
      app->setHtmlClass("");
    }

    if (app->environment().agentIsIElt(9) && container())
      container()->setOverflow(Overflow::Visible);
  }
}

int StdGridLayoutImpl2::minimumHeightForRow(int row) const
{
  int minHeight = 0;

  const unsigned colCount = grid_.columns_.size();
  for (unsigned j = 0; j < colCount; ++j) {
    WLayoutItem *item = grid_.items_[row][j].item_.get();
    if (item)
      minHeight = std::max(minHeight, getImpl(item)->minimumHeight());
  }

  return minHeight;
}

int StdGridLayoutImpl2::minimumWidthForColumn(int col) const
{
  int minWidth = 0;

  const unsigned rowCount = grid_.rows_.size();
  for (unsigned i = 0; i < rowCount; ++i) {
    WLayoutItem *item = grid_.items_[i][col].item_.get();
    if (item)
      minWidth = std::max(minWidth, getImpl(item)->minimumWidth());
  }

  return minWidth;
}

int StdGridLayoutImpl2::minimumWidth() const
{
  const unsigned colCount = grid_.columns_.size();

  int total = 0;

  for (unsigned i = 0; i < colCount; ++i)
    total += minimumWidthForColumn(i);

  return total + (colCount-1) * grid_.horizontalSpacing_;
}

int StdGridLayoutImpl2::minimumHeight() const
{
  const unsigned rowCount = grid_.rows_.size();

  int total = 0;

  for (unsigned i = 0; i < rowCount; ++i)
    total += minimumHeightForRow(i);

  return total + (rowCount-1) * grid_.verticalSpacing_;
}

void StdGridLayoutImpl2::itemAdded(WLayoutItem *item)
{
  addedItems_.push_back(item);
  update();
}

void StdGridLayoutImpl2::itemRemoved(WLayoutItem *item)
{
  Utils::erase(addedItems_, item);
  removedItems_.push_back(getImpl(item)->id());
  update();
}

void StdGridLayoutImpl2::update()
{
    WContainerWidget *c = container();

    if (c)
        c->layoutChanged(false);

    needConfigUpdate_ = true;
}

void StdGridLayoutImpl2
    ::streamConfig(WStringStream& js,
                   const std::vector<Impl::Grid::Section>& sections,
                   bool rows, WApplication *app)
{
    js << "[";

    for (unsigned i = 0; i < sections.size(); ++i) {
        if (i != 0)
            js << ",";

        js << "[" << sections[i].stretch_ << ",";

        if (sections[i].resizable_) {
            SizeHandle::loadJavaScript(app);

            js << "[";

            const WLength& size = sections[i].initialSize_;

            if (size.isAuto())
                js << "-1";
            else if (size.unit() == LengthUnit::Percentage)
                js << size.value() << ",1";
            else
                js << size.toPixels();

            js << "],";
        } else
            js << "0,";

        if (rows)
            js << minimumHeightForRow(i);
        else
            js << minimumWidthForColumn(i);

        js << "]";

    }

    js << "]";
}

void StdGridLayoutImpl2::streamConfig(fmt::memory_buffer &js,
                                      const std::vector<Impl::Grid::Section> &sections,
                                      bool rows,
                                      WApplication *app)
{
    js.push_back('[');
    for(unsigned i = 0; i < sections.size(); ++i) {
        auto& section = sections[i];
        auto minsize = rows ? minimumHeightForRow(i) : minimumWidthForColumn(i);

        if(section.resizable_) {
            SizeHandle::loadJavaScript(app);

            const WLength& size = section.initialSize_;
            if(size.unit() == LengthUnit::Percentage)
                fmt::format_to(std::back_inserter(js), "[{},[{},1],{}]", section.stretch_, size.value(), minsize);
            else
                fmt::format_to(std::back_inserter(js), "[{},[{}],{}]", section.stretch_, size.isAuto() ? -1 : size.toPixels(), minsize);

            // if(size.isAuto())
            //     fmt::format_to(std::back_inserter(js), "[{},[-1],{}]", section.stretch_, minsize);
            // else if(size.unit() == LengthUnit::Percentage)
            //     fmt::format_to(std::back_inserter(js), "[{},[{},1],{}]", section.stretch_, size.value(), minsize);
            // else
            //     fmt::format_to(std::back_inserter(js), "[{},[{}],{}]", section.stretch_, size.toPixels(), minsize);
        }
        else
            fmt::format_to(std::back_inserter(js), "[{},0,{}]", section.stretch_, minsize);

        if(i != sections.size() - 1)
            js.push_back(',');
    }
    js.push_back(']');
}

void StdGridLayoutImpl2::streamConfig(WStringStream& js, WApplication *app)
{
  js << "{ rows:";

  streamConfig(js, grid_.rows_, true, app);

  js << ", cols:";

  streamConfig(js, grid_.columns_, false, app);

  js << ", items: [";

  const unsigned colCount = grid_.columns_.size();
  const unsigned rowCount = grid_.rows_.size();

  for (unsigned row = 0; row < rowCount; ++row) {
      for (unsigned col = 0; col < colCount; ++col) {
          Impl::Grid::Item& item = grid_.items_[row][col];

          AlignmentFlag hAlign = item.alignment_ & AlignHorizontalMask;
          AlignmentFlag vAlign = item.alignment_ & AlignVerticalMask;

          if (row + col != 0)
              js << ",";

          if (item.item_) {
              std::string id = getImpl(item.item_.get())->id();

              js << "{";

              if (item.colSpan_ != 1 || item.rowSpan_ != 1)
                  js << "span: [" << item.colSpan_ << "," << item.rowSpan_ << "],";

              if (item.alignment_.value()) {
                  unsigned align = 0;

                  if (hAlign != static_cast<AlignmentFlag>(0))
                      switch (hAlign) {
                      case AlignmentFlag::Left: align |= 0x1; break;
                      case AlignmentFlag::Right: align |= 0x2; break;
                      case AlignmentFlag::Center: align |= 0x4; break;
                      default: break;
                      }

                  if (vAlign != static_cast<AlignmentFlag>(0))
                      switch (vAlign) {
                      case AlignmentFlag::Top: align |= 0x10; break;
                      case AlignmentFlag::Bottom: align |= 0x20; break;
                      case AlignmentFlag::Middle: align |= 0x40; break;
                      default: break;
                      }

                  js << "align:" << (int)align << ",";
              }

              js << "dirty:" << (grid_.items_[row][col].update_ ? 2 : 0)
                 << ",id:'" << id << "'"
                 << "}";

              grid_.items_[row][col].update_ = 0;
          } else
              js << "null";
      }
  }

  js << "]}";
}

void StdGridLayoutImpl2::streamConfig(fmt::memory_buffer &js, WApplication */*app*/, std::string_view closing)
{
    fmt::format_to(std::back_inserter(js), "{{ rows:{:r}, cols:{:c}, items: [{}]}}{}", this, this, grid_, closing);
}

int StdGridLayoutImpl2::pixelSize(const WLength& size)
{
  if (size.unit() == LengthUnit::Percentage)
    return 0;
  else
    return (int)size.toPixels();
}

/*
 * fitWidth, fitHeight:
 *  - from setLayout(AlignmentFlag::Left | AlignmentFlag::Top)
 *    is being deprecated but still needs to be implemented
 *  - nested layouts: handles as other layout items
 */
#warning "not sure about copy elison here"
DomElement StdGridLayoutImpl2::createDomElement(DomElement *parent,
                                                bool fitWidth, bool fitHeight,
                                                WApplication *app)
{
    needAdjust_ = needConfigUpdate_ = needRemeasure_ = false;
    addedItems_.clear();
    removedItems_.clear();

    const unsigned colCount = grid_.columns_.size();
    const unsigned rowCount = grid_.rows_.size();

    int margin[] = { 0, 0, 0, 0};

    int maxWidth = 0, maxHeight = 0;

    if (layout()->parentLayout() == nullptr) {
        /*
     * If it is a top-level layout (as opposed to a nested layout),
     * configure overflow of the container.
     */
        if (container() == app->root()) {
            /*
       * Reset body,html default paddings and so on if we are doing layout
       * in the entire document.
       */
            app->setBodyClass(app->bodyClass() + " Wt-layout");
            app->setHtmlClass(app->htmlClass() + " Wt-layout");
        }

#ifndef WT_TARGET_JAVA
        layout()->getContentsMargins(margin + 3, margin, margin + 1, margin + 2);
#else // WT_TARGET_JAVA
        margin[3] = layout()->getContentsMargin(Side::Left);
        margin[0] = layout()->getContentsMargin(Side::Top);
        margin[1] = layout()->getContentsMargin(Side::Right);
        margin[2] = layout()->getContentsMargin(Side::Bottom);
#endif // WT_TARGET_JAVA

        maxWidth = pixelSize(container()->maximumWidth());
        maxHeight = pixelSize(container()->maximumHeight());
    }

    fmt::memory_buffer js;
    bool progressive = !app->environment().ajax();
    if (layout()->parentLayout() &&
        dynamic_cast<StdGridLayoutImpl2*>(getImpl(layout()->parentLayout())))
        fmt::format_to(std::back_inserter(js), "{}.layouts2.add(new {}.StdLayout2({},'{}','{}',{:d},{:d},{:d},{},{},[{},{},{}],[{},{},{}],",
                       app->javaScriptClass(),
                       WT_CLASS,
                       app->javaScriptClass(),
                       id(), getImpl(layout()->parentLayout())->id(),
                       fitWidth, fitHeight, progressive,
                       maxWidth, maxHeight,
                       grid_.horizontalSpacing_, margin[3], margin[1],
                       grid_.verticalSpacing_, margin[0], margin[2]);
    else {
        fmt::format_to(std::back_inserter(js), "{}.layouts2.add(new {}.StdLayout2({},'{}',null,{:d},{:d},{:d},{},{},[{},{},{}],[{},{},{}],",
                       app->javaScriptClass(),
                       WT_CLASS,
                       app->javaScriptClass(),
                       id(),
                       fitWidth, fitHeight, progressive,
                       maxWidth, maxHeight,
                       grid_.horizontalSpacing_, margin[3], margin[1],
                       grid_.verticalSpacing_, margin[0], margin[2]);
    }

    // WStringStream js;

    // js << app->javaScriptClass()
    //    << ".layouts2.add(new " WT_CLASS ".StdLayout2("
    //    << app->javaScriptClass() << ",'"
    //    << id() << "',";

    // if (layout()->parentLayout() &&
    //     dynamic_cast<StdGridLayoutImpl2*>(getImpl(layout()->parentLayout())))
    //     js << "'" << getImpl(layout()->parentLayout())->id() << "',";
    // else
    //     js << "null,";

    // bool progressive = !app->environment().ajax();
    // js << (fitWidth ? '1' : '0') << "," << (fitHeight ? '1' : '0') << ","
    //    << (progressive ? '1' : '0') << ",";

    // js << maxWidth << "," << maxHeight
    //    << ",["
    //    << grid_.horizontalSpacing_ << "," << margin[3] << "," << margin[1]
    //    << "],["
    //    << grid_.verticalSpacing_ << "," << margin[0] << "," << margin[2] << "],";

    streamConfig(js, app, "));");
    //fmt::format_to(std::back_inserter(js), "));");

    //DomElement *div = DomElement::createNew(DomElementType::DIV);
    DomElement div(DomElement::Mode::Create, DomElementType::DIV);
    div.setId(id());
    div.setProperty(Property::StylePosition, "relative");

    DomElement *tableptr = nullptr, *tbodyptr = nullptr, *trptr = nullptr;

    DomElement table = DomElement::createNew(DomElementType::TABLE);
    DomElement tbody = DomElement::createNew(DomElementType::TBODY);
    if (progressive) {

        tableptr = &table;
        // WStringStream style;
        // if (maxWidth)
        //     style << "max-width: " << maxWidth << "px;";
        // if (maxHeight)
        //     style << "max-height: " << maxHeight << "px;";
        // style << "width: 100%;";
        if(maxWidth && maxHeight)
            table.setProperty(Property::Style, fmt::format("max-width: {}px;max-height: {}px;width: 100%;", maxWidth, maxHeight));
        else if(maxWidth)
            table.setProperty(Property::Style, fmt::format("max-width: {}px;width: 100%;", maxWidth));
        else if(maxHeight)
            table.setProperty(Property::Style, fmt::format("max-height: {}px;width: 100%;", maxHeight));
        else
            table.setProperty(Property::Style, "width: 100%;");
        //table->setProperty(Property::Style, style.str());

        int totalColStretch = 0;
        for (unsigned col = 0; col < colCount; ++col)
            totalColStretch += std::max(0, grid_.columns_[col].stretch_);

        for (unsigned col = 0; col < colCount; ++col) {
            DomElement c(DomElement::Mode::Create, DomElementType::COL);// = DomElement::createNew(DomElementType::COL);
            int stretch = std::max(0, grid_.columns_[col].stretch_);

            if (stretch || totalColStretch == 0) {
                char buf[30];

                double pct = totalColStretch == 0 ? 100.0 / colCount
                                                  : (100.0 * stretch / totalColStretch);

                // WStringStream ss;
                // ss << "width:" << Utils::round_css_str(pct, 2, buf) << "%;";
                c.setProperty(Property::Style, fmt::format("width:{}%;", Utils::round_css_str(pct, 2, buf)));
            }

            table.addChild(c);
        }

        //tbody = DomElement::createNew(DomElementType::TBODY);
        tbodyptr = &tbody;
    }

#ifndef WT_TARGET_JAVA
    std::vector<bool> overSpanned(colCount * rowCount, false);
#else
    std::vector<bool> overSpanned;
    overSpanned.insert(0, colCount * rowCount, false);
#endif // WT_TARGET_JAVA

    int prevRowWithItem = -1;

    for (unsigned row = 0; row < rowCount; ++row) {
        DomElement tr = DomElement::createNew(DomElementType::TR);
        if (tableptr)
            trptr = &tr;

        bool rowVisible = false;
        int prevColumnWithItem = -1;

        for (unsigned col = 0; col < colCount; ++col) {
            Impl::Grid::Item& item = grid_.items_[row][col];

            if (!overSpanned[row * colCount + col]) {
                for (int i = 0; i < item.rowSpan_; ++i)
                    for (int j = 0; j < item.colSpan_; ++j)
                        if (i + j > 0)
                            overSpanned[(row + i) * colCount + col + j] = true;

                AlignmentFlag hAlign = item.alignment_ & AlignHorizontalMask;
                AlignmentFlag vAlign = item.alignment_ & AlignVerticalMask;

                //DomElement *td = nullptr;
                DomElement td = DomElement::createNew(DomElementType::TD);

                if (tableptr) {
                    bool itemVisible = hasItem(row, col);
                    rowVisible = rowVisible || itemVisible;

                    //DomElement *td = DomElement::createNew(DomElementType::TD);

                    if (itemVisible) {
                        int padding[] = { 0, 0, 0, 0 };

                        int nextRow = nextRowWithItem(row, col);
                        int prevRow = prevRowWithItem;

                        int nextCol = nextColumnWithItem(row, col);
                        int prevCol = prevColumnWithItem;

                        if (prevRow == -1)
                            padding[0] = margin[0];
                        else
                            padding[0] = (grid_.verticalSpacing_+1) / 2;

                        if (nextRow == (int)rowCount)
                            padding[2] = margin[2];
                        else
                            padding[2] = grid_.verticalSpacing_ / 2;

                        if (prevCol == -1)
                            padding[3] = margin[3];
                        else
                            padding[3] = (grid_.horizontalSpacing_ + 1)/2;

                        if (nextCol == (int)colCount)
                            padding[1] = margin[1];
                        else
                            padding[1] = (grid_.horizontalSpacing_)/2;

                        fmt::memory_buffer style;

                        if (app->layoutDirection() == LayoutDirection::RightToLeft)
                            std::swap(padding[1], padding[3]);

                        if (padding[0] == padding[1] && padding[0] == padding[2]
                            && padding[0] == padding[3]) {
                            if (padding[0] != 0)
                                fmt::format_to(std::back_inserter(style) , "padding:{}px;{:v}", padding[0], vAlign);
                        } else
                            fmt::format_to(std::back_inserter(style), "padding:{}px {}px {}px {}px;{:v}", padding[0], padding[1], padding[2], padding[3], vAlign);
                            // style << "padding:"
                            //       << padding[0] << "px " << padding[1] << "px "
                            //       << padding[2] << "px " << padding[3] << "px;";

                        // if (static_cast<unsigned int>(vAlign) != 0)
                        //     switch (vAlign) {
                        //     case AlignmentFlag::Top:
                        //         style << "vertical-align:top;";
                        //         break;
                        //     case AlignmentFlag::Middle:
                        //         style << "vertical-align:middle;";
                        //         break;
                        //     case AlignmentFlag::Bottom:
                        //         style << "vertical-align:bottom;";
                        //     default:
                        //         break;
                        //     }

                        td.setProperty(Property::Style, fmt::to_string(style));

                        if (item.rowSpan_ != 1)
                            td.setProperty(Property::RowSpan,
                                            std::to_string(item.rowSpan_));
                        if (item.colSpan_ != 1)
                            td.setProperty(Property::ColSpan,
                                            std::to_string(item.colSpan_));

                        prevColumnWithItem = col;
                    }
                }

                DomElement *c = nullptr;

                DomElement cc = !tableptr && item.item_ ? createElement(item.item_.get(), app) : getImpl(item.item_.get())->createDomElement(nullptr, true, true, app);
                DomElement itable = DomElement::createNew(DomElementType::TABLE);
                if(item.item_) {
                    c = &cc;
                }

                // if (!tableptr) {
                //     if (item.item_) {
                //         c = createElement(item.item_.get(), app);
                //         div.addChild(c);
                //     }
                // } else{
                //     if (item.item_) {
                //         c = getImpl(item.item_.get())->createDomElement(nullptr, true, true, app);
                //     }
                // }

                if (tableptr) {
                    if (c) {
                        if (!app->environment().agentIsIElt(9))
                            c->setProperty(Property::StyleBoxSizing, "border-box");

                        if (static_cast<unsigned int>(hAlign) == 0)
                            hAlign = AlignmentFlag::Justify;

                        switch (hAlign) {
                        case AlignmentFlag::Center: {
                            //DomElement itable = DomElement::createNew(DomElementType::TABLE);
                            itable.setProperty(Property::Class, "Wt-hcenter");
                            if (static_cast<unsigned int>(vAlign) == 0)
                                itable.setProperty(Property::Style, "height:100%;");
                            DomElement irow = DomElement::createNew(DomElementType::TR);
                            DomElement itd = DomElement::createNew(DomElementType::TD);
                            if (static_cast<unsigned int>(vAlign) == 0)
                                itd.setProperty(Property::Style, "height:100%;");

                            bool haveMinWidth
                                = !c->getProperty(Property::StyleMinWidth).empty();

                            itd.addChild(cc);

                            if (app->environment().agentIsIElt(9)) {
                                // IE7 and IE8 do support min-width but do not enforce it
                                // properly when in a table.
                                //  see http://stackoverflow.com/questions/2356525
                                //            /css-min-width-in-ie6-7-and-8
                                if (haveMinWidth) {
                                    DomElement spacer = DomElement::createNew(DomElementType::DIV);
                                    spacer.setProperty(Property::StyleWidth,
                                                        c->getProperty(Property::StyleMinWidth));
                                    spacer.setProperty(Property::StyleHeight, "1px");
                                    itd.addChild(spacer);
                                }
                            }

                            irow.addChild(itd);
                            itable.addChild(irow);
                            c = &itable;
                            break;
                        }
                        case AlignmentFlag::Right:
                            if (!c->isDefaultInline())
                                c->setProperty(Property::StyleFloat, "right");
                            else
                                td.setProperty(Property::StyleTextAlign, "right");
                            break;
                        case AlignmentFlag::Left:
                            if (!c->isDefaultInline())
                                c->setProperty(Property::StyleFloat, "left");
                            else
                                td.setProperty(Property::StyleTextAlign, "left");
                            break;
                        default:
                            break;
                        }

                        bool haveMinWidth
                            = !c->getProperty(Property::StyleMinWidth).empty();

                        td.addChild(*c);

                        if (app->environment().agentIsIElt(9)) {
                            // IE7 and IE8 do support min-width but do not enforce it properly
                            // when in a table.
                            //  see http://stackoverflow.com/questions/2356525
                            //            /css-min-width-in-ie6-7-and-8
                            if (haveMinWidth) {
                                DomElement spacer = DomElement::createNew(DomElementType::DIV);
                                spacer.setProperty(Property::StyleWidth,
                                                    c->getProperty(Property::StyleMinWidth));
                                spacer.setProperty(Property::StyleHeight, "1px");
                                td.addChild(spacer);
                            }
                        }
                    }

                    tr.addChild(td);
                }
                else if(item.item_) { //if (!tableptr) { if (item.item_) {
                    div.addChild(cc);
                }
            }
        }

        if (trptr) {
            if (!rowVisible)
                tr.setProperty(Property::StyleDisplay, "hidden");
            else
                prevRowWithItem = row;
            tbody.addChild(tr);
        }
    }

    //js << "));";

    if (tableptr) {
        table.addChild(tbody);
        div.addChild(table);
    }
#warning "Maybe directly format the member of the domelement ?"
    div.callJavaScript(fmt::to_string(js));

    if (layout()->parentLayout() == nullptr) {
        WContainerWidget *c = container();

        /*
         * Take the hint: if the container is relative, then we can use an absolute
         * layout for its contents, under the assumption that a .wtResize or
         * auto-javascript sets the width too (like in WTreeView, WTableView)
         */
        if (c->positionScheme() == PositionScheme::Relative ||
            c->positionScheme() == PositionScheme::Absolute) {
            div.setProperty(Property::StylePosition, "absolute");
            div.setProperty(Property::StyleLeft, "0");
            div.setProperty(Property::StyleRight, "0");
        } else if (app->environment().agentIsIE()) {
            /*
           * position: relative element needs to be in a position: relative
           * parent otherwise scrolling is broken
           */
            if (app->environment().agentIsIE()
                && c->parent()->positionScheme() != PositionScheme::Static)
                parent->setProperty(Property::StylePosition, "relative");
        }

        AlignmentFlag hAlign = c->contentAlignment() & AlignHorizontalMask;
        switch (hAlign) {
        case AlignmentFlag::Center: {
            DomElement itable = DomElement::createNew(DomElementType::TABLE);
            itable.setProperty(Property::Class, "Wt-hcenter");
            if (fitHeight)
                itable.setProperty(Property::Style, "height:100%;");
            DomElement irow = DomElement::createNew(DomElementType::TR);
            DomElement itd = DomElement::createNew(DomElementType::TD);
            if (fitHeight)
                itd.setProperty(Property::Style, "height:100%;");
            itd.addChild(div);
            irow.addChild(itd);
            itable.addChild(irow);
            itable.setId(id() + "l");
            return itable; //  copy elision?? ////div = itable;

            break;
        }
        case AlignmentFlag::Left:
            break;
        case AlignmentFlag::Right:
            div.setProperty(Property::StyleFloat, "right");
            break;
        default:
            break;
        }
    }

    return div; //Since div is a single named object modified throughout the function and returned in the default case, modern compilers (e.g., GCC, Clang, MSVC) are likely to apply NRVO, constructing div directly in the caller’s memory.

}

} // namespace Wt
