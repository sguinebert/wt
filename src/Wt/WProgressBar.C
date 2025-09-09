/*
 * Copyright (C) 2010 Thomas Suckow.
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 *   progressCompleted() and valueChanged() contributed by Omer Katz.
 *
 * See the LICENSE file for terms of use.
 */

#include <Wt/WApplication.h>
#include <Wt/WBootstrap5Theme.h>
#include <Wt/WProgressBar.h>
#include <Wt/WTheme.h>

#include "DomElement.h"
#include "WebUtils.h"

using namespace Wt;

namespace Wt {

WProgressBar::WProgressBar()
  : min_(0),
    max_(100),
    value_(0),
    changed_(false)
{
  format_ = WString::fromUTF8("%.0f %%");
  setFlexBox(true);
  setInline(true);
}

void WProgressBar::setValueStyleClass(const std::string& valueStyleClass)
{
  valueStyleClass_ = valueStyleClass;
}

awaitable<void> WProgressBar::setValue(double value)
{
  value_ = value;

  co_await valueChanged_.emit(value_);
  
  if (value_ == max_)
    co_await progressCompleted_.emit();
  
  changed_ = true;
  repaint();
}

void WProgressBar::setMinimum(double minimum)
{
  min_ = minimum;

  changed_ = true;
  repaint();
}

void WProgressBar::setMaximum(double maximum)
{
  max_ = maximum;

  changed_ = true;
  repaint();
}

void WProgressBar::setRange(double minimum, double maximum)
{
  min_ = minimum;
  max_ = maximum;

  changed_ = true;
  repaint();
}

void WProgressBar::setState(double minimum, double maximum, double value)
{
  min_ = minimum;
  max_ = maximum;

  if (value_ != value) {
    value_ = value;

//    if (value_ == max_)
//      co_await progressCompleted_.emit();
  }
}

void WProgressBar::setFormat(const WString& format)
{
  format_ = format;
}

WString WProgressBar::text() const
{
  return Utils::formatFloat(format_, percentage());
}

double WProgressBar::percentage() const
{
  double max = maximum(), min = minimum();

  if (max - min != 0)
    return (value() - min) * 100 / (max - min);
  else
    return 0;
}

DomElementType WProgressBar::domElementType() const
{
  return DomElementType::DIV; // later support DomElementType::PROGRESS
}

void WProgressBar::resize(const WLength& width, const WLength& height)
{
  WInteractWidget::resize(width, height);

  if (!height.isAuto())
    setAttributeValue("style", "line-height: " + height.cssText());
}

void WProgressBar::updateBar(DomElement& bar)
{
  bar.setProperty(Property::StyleWidth, std::to_string(percentage()) + "%");
}

void WProgressBar::updateDom(DomElement& element, bool all)
{
  //DomElement bar = changed_ ? DomElement::getForUpdate("bar" + id(), DomElementType::DIV) :DomElement::createNew(DomElementType::DIV);
  //DomElement label = changed_ ? DomElement::getForUpdate("lbl" + id(), DomElementType::DIV) : DomElement::createNew(DomElementType::DIV);
  DomElement *barptr = nullptr, *labelptr = nullptr;

  auto app = WApplication::instance();
  auto bs5Theme = std::dynamic_pointer_cast<Wt::WBootstrap5Theme>(app->theme());

  auto& bar = changed_ ? element.addChild("bar" + id(), DomElementType::DIV) : element.addChild(DomElementType::DIV);
  auto& label = bs5Theme ? bar : changed_ ? element.addChild("lbl" + id(), DomElementType::DIV) : element.addChild(DomElementType::DIV);

  if (all) {
#ifndef WT_NO_WASM
    element.tryEmplaceAttribute("data-wt", "WProgressBar");
#endif // WT_NO_WASM
    barptr = &bar;// DomElement::createNew(DomElementType::DIV);
    barptr->setId("bar" + id());
    barptr->setProperty(Property::Class, valueStyleClass_);
    app->theme()->apply(this, bar, ProgressBarBar);

    if (bs5Theme) {
      labelptr = barptr;
    } else {
      labelptr = &label; //DomElement::createNew(DomElementType::DIV);
      labelptr->setId("lbl" + id());
      app->theme()->apply(this, label, ProgressBarLabel);
    }
  }

  if (changed_ || all) {
    if (!barptr)
      barptr = &bar;//DomElement::getForUpdate("bar" + id(), DomElementType::DIV);
    if (!labelptr) {
      if (bs5Theme) {
        labelptr = barptr;
      } else {
        labelptr = &label;//DomElement::getForUpdate("lbl" + id(), DomElementType::DIV);
      }
    }

    updateBar(bar);

    WString s = text();
    removeScript(s);

    labelptr->setProperty(Property::InnerHTML, s.toUTF8());

    changed_ = false;
  }

  // if (barptr)
  //   element.addChild(bar);

  // if (labelptr && !bs5Theme)
  //   element.addChild(label);

  WInteractWidget::updateDom(element, all);
}

void WProgressBar::propagateRenderOk(bool deep)
{
  changed_ = false;

  WInteractWidget::propagateRenderOk(deep);
}

}

