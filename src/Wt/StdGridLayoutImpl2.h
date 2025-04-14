// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#ifndef STD_GRID_LAYOUT_IMPL2_H_
#define STD_GRID_LAYOUT_IMPL2_H_

#include <Wt/WGridLayout.h>
#include "StdLayoutImpl.h"

// Specialize fmt::formatter for AlignmentFlag
template <>
struct fmt::formatter<Wt::AlignmentFlag>
{
    char type = 'v'; // Default to 'r' if no specifier is given
    // Parse format specifiers (no-op for this simple case)
    constexpr auto parse(fmt::format_parse_context& ctx) {
        auto it = ctx.begin(); // Start of the format specifier
        auto end = ctx.end();  // End of the format specifier

        // Check if there's a specifier and it's 'c' or 'r'
        if (it != end && (*it == 'v' || *it == 'h')) {
            type = *it; // Store the specifier ('c' or 'r')
            ++it;       // Move past the specifier
        } else if (it != end && *it != '}') {
            // If there's something but it's not 'c', 'r', or '}', it’s invalid
            throw fmt::format_error("invalid format specifier; expected 'v' or 'h'");
        }
        return it; // Return the iterator position after parsing
    }

    // Format the enum value as a string
    template <typename FormatContext>
    auto format(Wt::AlignmentFlag flag, FormatContext& ctx) const {
        std::string_view name;
        switch (static_cast<int>(flag)) {
        case 0x1:    name = "Left"; break;
        case 0x2:    name = "Right"; break;
        case 0x4:    name = "Center"; break;
        case 0x8:    name = "Justify"; break;
        case 0x10:   name = "Baseline"; break;
        case 0x20:   name = "Sub"; break;
        case 0x40:   name = "Super"; break;
        case 0x80:   name = "Top"; break;
        case 0x100:  name = "TextTop"; break;
        case 0x200:  name = "Middle"; break;
        case 0x400:  name = "Bottom"; break;
        case 0x800:  name = "TextBottom"; break;
        default:
            return fmt::format_to(ctx.out(), "");
        }
        std::string_view pos;
        switch (type) {
        case 'v': pos = "vertical-align"; break;
        case 'h': pos = "horizontal-align"; break;

        default:
            return fmt::format_to(ctx.out(), "{}", name);
        }

        return fmt::format_to(ctx.out(), "{}:{};", pos, name);
    }
};

namespace Wt {

  class WApplication;
  class WLayout;
  class WStringStream;

class StdGridLayoutImpl2 : public StdLayoutImpl
{
public:
  StdGridLayoutImpl2(WLayout *layout, Impl::Grid& grid);
  virtual ~StdGridLayoutImpl2();

  virtual int minimumWidth() const override;
  virtual int minimumHeight() const override;

  virtual void itemAdded(WLayoutItem *) override;
  virtual void itemRemoved(WLayoutItem *) override;

  virtual void updateDom(DomElement& parent) override;

  virtual void update() override;

  virtual DomElement createDomElement(DomElement *parent,
                                      bool fitWidth, bool fitHeight,
                                      WApplication *app) override;

  // Does not really belong here, but who cares ?
  static const char* childrenResizeJS();

  virtual bool itemResized(WLayoutItem *item) override;
  virtual bool parentResized() override;

private:
  Impl::Grid& grid_;
  bool needAdjust_, needRemeasure_, needConfigUpdate_;
  std::vector<WLayoutItem *> addedItems_;
  std::vector<std::string> removedItems_;

  int nextRowWithItem(int row, int c) const;
  int nextColumnWithItem(int row, int col) const;
  bool hasItem(int row, int col) const;
  int minimumHeightForRow(int row) const;
  int minimumWidthForColumn(int column) const;
  static int pixelSize(const WLength& size);

  void streamConfig(WStringStream& js,
                    const std::vector<Impl::Grid::Section>& sections,
                    bool rows, WApplication *app);
  void streamConfig(fmt::memory_buffer& js,
                    const std::vector<Impl::Grid::Section>& sections,
                    bool rows, WApplication *app);
  void streamConfig(WStringStream& js, WApplication *app);
  void streamConfig(fmt::memory_buffer& js, WApplication *app, std::string_view closing = "");
  DomElement createElement(WLayoutItem *item, WApplication *app);

  friend struct fmt::formatter<Wt::StdGridLayoutImpl2>;
  friend struct fmt::formatter<const Wt::StdGridLayoutImpl2>;
};

}

#endif // STD_GRID_LAYOUT_IMPL2_H_
