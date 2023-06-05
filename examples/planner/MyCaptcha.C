/*
 * Copyright (C) 2010 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include "MyCaptcha.h"

MyCaptcha::MyCaptcha(const int width, const int height)
  : WContainerWidget()
{
  setStyleClass("captcha");
		
  captchaMessage_ = this->addWidget(std::make_unique<WText>());

  shapesWidget_ = this->addWidget(std::make_unique<ShapesWidget>());
  shapesWidget_->resize(width, height);

  shapesWidget_->clicked().connect(this, &MyCaptcha::handleClick);

  regenerate();
}
  
awaitable<void> MyCaptcha::handleClick(const WMouseEvent& me)
{
  if (shapesWidget_->correctlyClicked(me)) 
    co_await completed_.emit();
  else
    regenerate();
  co_return;
}

void MyCaptcha::regenerate()
{
  shapesWidget_->initShapes();
  shapesWidget_->update();
  captchaMessage_->setText(tr("captcha.message")
			   .arg(shapesWidget_->selectedColor())
			   .arg(shapesWidget_->selectedShape()));
}
