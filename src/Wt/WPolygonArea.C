/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include "Wt/WPolygonArea.h"
#include "Wt/WPointF.h"

#include "DomElement.h"
#include "WebUtils.h"

namespace fmt {

// template <>
// struct formatter<const Wt::WPointF> {
//     constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

//   template <typename FormatContext>
//   auto format(const Wt::WPointF& p, FormatContext& ctx) const {
//     return fmt::format_to(ctx.out(), "{:.0},{:.0}", p.x(), p.y());
//   }
// };
template <>
struct formatter<Wt::WPointF> {
    // Parse the format specifiers (if any).
    constexpr auto parse(fmt::format_parse_context& ctx) {
        return ctx.begin();
    }
    // Format the Wt::WPointF. Adjust the formatting as needed.
    template <typename FormatContext>
    auto format(const Wt::WPointF& point, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{:.0},{:.0}", point.x(), point.y());
    }
};

}

namespace Wt {

WPolygonArea::WPolygonArea()
{ }

WPolygonArea::WPolygonArea(const std::vector<WPoint>& points)
{
  setPoints(points);
}

#ifndef WT_TARGET_JAVA
WPolygonArea::WPolygonArea(const std::vector<WPointF>& points)
 : points_(points)
{ }
#endif // WT_TARGET_JAVA

void WPolygonArea::addPoint(int x, int y)
{
  points_.push_back(WPointF(x, y));
  
  repaint();
}

void WPolygonArea::addPoint(double x, double y)
{
  points_.push_back(WPointF(x, y));
  
  repaint();
}

void WPolygonArea::addPoint(const WPoint& point)
{
  points_.push_back(WPointF(point.x(), point.y()));
  
  repaint();
}

void WPolygonArea::addPoint(const WPointF& point)
{
  points_.push_back(point);
               
  repaint();
}

void WPolygonArea::setPoints(const std::vector<WPoint>& points)
{
  points_.clear();
  for (unsigned i = 0; i < points.size(); ++i)
    addPoint(points[i]);
  
  repaint();
}

#ifndef WT_TARGET_JAVA
void WPolygonArea::setPoints(const std::vector<WPointF>& points)
{
  points_ = points;

  repaint();
}
#endif // WT_TARGET_JAVA

bool WPolygonArea::updateDom(DomElement& element, bool all)
{
  element.setAttribute("shape", "poly", true);

  // std::stringstream coords;
  // for (unsigned i = 0; i < points_.size(); ++i) {
  //   if (i != 0)
  //     coords << ',';
  //   coords << static_cast<int>(points_[i].x()) << ','
  //          << static_cast<int>(points_[i].y());
  // }
  ;
  element.setAttribute("coords", fmt::format("{}", fmt::join(points_, ",")), true);

  return WAbstractArea::updateDom(element, all);
}

std::string WPolygonArea::updateAreaCoordsJS()
{
  std::stringstream coords;
  char buf[30];

  coords << "[" << jsRef() << ",[";
  for (unsigned i = 0; i < points_.size(); ++i) {
    if (i != 0)
      coords << ',';
    coords << Utils::round_js_str(points_[i].x(), 2, buf) << ',';
    coords << Utils::round_js_str(points_[i].y(), 2, buf);
  }
  coords << "]]";

  return coords.str();
}

/*
 * NOTE: points() does not receive updates, unlike pointsF()
 */
const std::vector<WPoint>& WPolygonArea::points() const
{
  pointsIntCompatibility_.clear();
  for (unsigned i = 0; i < points_.size(); ++i)
    pointsIntCompatibility_.push_back(WPoint(static_cast<int>(points_[i].x()), static_cast<int>(points_[i].y())));

  return pointsIntCompatibility_;
}

}
