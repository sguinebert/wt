/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include "Wt/WApplication.h"
#include "Wt/WBootstrap5Theme.h"
#include "Wt/WInPlaceEdit.h"
#include "Wt/WCssDecorationStyle.h"
#include "Wt/WContainerWidget.h"
#include "Wt/WPushButton.h"
#include "Wt/WText.h"
#include "Wt/WTheme.h"
#include "Wt/WLineEdit.h"

#include <memory>

namespace Wt {

WInPlaceEdit::WInPlaceEdit()
{
  create();
}

WInPlaceEdit::WInPlaceEdit(const WString& text)
{
  create();
  setText(text);
}

WInPlaceEdit::WInPlaceEdit(bool buttons, const WString& text)
{
  create();
  setText(text);
  setButtonsEnabled(buttons);
}

void WInPlaceEdit::create()
{
  setImplementation(std::unique_ptr<WWidget>(impl_ = new WContainerWidget()));
  setInline(true);

  impl_->addWidget(std::unique_ptr<WWidget>
		   (text_ = new WText(WString::Empty, TextFormat::Plain)));
  text_->decorationStyle().setCursor(Cursor::Arrow);

  impl_->addWidget(std::unique_ptr<WWidget>(editing_ = new WContainerWidget()));
  editing_->setInline(true);
  editing_->hide();

  editing_->addWidget(std::unique_ptr<WWidget>(edit_ = new WLineEdit()));
  edit_->setTextSize(20);
  save_ = nullptr;
  cancel_ = nullptr;

  /*
   * This is stateless implementation heaven
   */
  text_->clicked().connect<&WWidget::hide>(text_);
  text_->clicked().connect<&WWidget::show>(editing_);
  text_->clicked().connect<&WWidget::focus>(edit_);

  edit_->enterPressed().connect<&WFormWidget::disable>(edit_);
  edit_->enterPressed().connect<&WInPlaceEdit::save>(this);
  edit_->enterPressed().preventPropagation();

  edit_->escapePressed().connect<&WWidget::hide>(editing_);
  edit_->escapePressed().connect<&WWidget::show>(text_);
  edit_->escapePressed().connect<&WInPlaceEdit::cancel>(this);
  edit_->escapePressed().preventPropagation();

  auto app = WApplication::instance();
  auto bs5Theme = std::dynamic_pointer_cast<WBootstrap5Theme>(app->theme());

  if (!bs5Theme) {
    editing_->addWidget
      (std::unique_ptr<WWidget>(buttons_ = new WContainerWidget()));
    buttons_->setInline(true);

    app->theme()->apply(this, buttons_, InPlaceEditingButtonsContainer);
  }

  setButtonsEnabled();
}

const WString& WInPlaceEdit::text() const
{
  return edit_->text();
}

void WInPlaceEdit::setText(const WString& text)
{
  empty_ = text.empty();

  if (!empty_)
    text_->setText(text);
  else
    text_->setText(placeholderText());

  edit_->setText(text);
}

void WInPlaceEdit::setPlaceholderText(const WString& text)
{
  placeholderText_ = text;

  edit_->setPlaceholderText(text);
  if (empty_)
    text_->setText(text);
}

const WString& WInPlaceEdit::placeholderText() const
{
  return placeholderText_;
}

awaitable<void> WInPlaceEdit::save()
{
  editing_->hide();
  text_->show();
  edit_->enable();
  if (save_)
    save_->enable();
  if (cancel_)
    cancel_->enable();

  bool changed = empty_ ? !edit_->text().empty() : edit_->text() != text_->text();

  if (changed) {
    setText(edit_->text());
    co_await valueChanged().emit((WString&)edit_->text());
  }
  co_return;
}

void WInPlaceEdit::cancel()
{
  edit_->setText(empty_ ? WString::Empty : text_->text());
}

void WInPlaceEdit::setButtonsEnabled(bool enabled)
{
  if (enabled && !save_) {
    //c2_.disconnect();
    edit_->blurred().disconnect<&WInPlaceEdit::save>(this);

    auto app = WApplication::instance();
    auto bs5Theme = std::dynamic_pointer_cast<WBootstrap5Theme>(app->theme());

    if (!bs5Theme) {
      buttons_->addWidget
        (std::unique_ptr<WWidget>
         (save_ = new WPushButton(tr("Wt.WInPlaceEdit.Save"))));
      buttons_->addWidget
        (std::unique_ptr<WWidget>
         (cancel_ = new WPushButton(tr("Wt.WInPlaceEdit.Cancel"))));
    } else {
      editing_->addWidget (std::unique_ptr<WWidget>
       (save_ = new WPushButton(tr("Wt.WInPlaceEdit.Save"))));
      editing_->addWidget (std::unique_ptr<WWidget>
       (cancel_ = new WPushButton(tr("Wt.WInPlaceEdit.Cancel"))));
    }

    app->theme()->apply(this, save_, InPlaceEditingButton);
    app->theme()->apply(this, cancel_, InPlaceEditingButton);

    save_->clicked().connect<&WFormWidget::disable>(edit_);
    save_->clicked().connect<&WFormWidget::disable>(save_);
    save_->clicked().connect<&WFormWidget::disable>(cancel_);
    save_->clicked().connect<&WInPlaceEdit::save>(this);
    
    cancel_->clicked().connect<&WWidget::hide>(editing_);
    cancel_->clicked().connect<&WWidget::show>(text_);
    cancel_->clicked().connect<&WInPlaceEdit::cancel>(this);
  } else if (!enabled && save_) {
    save_->parent()->removeWidget(save_);
    cancel_->parent()->removeWidget(cancel_);
    save_ = nullptr;
    cancel_ = nullptr;

//#warning "c2_ "
    //c2_ ;
    edit_->blurred().connect<&WInPlaceEdit::save>(this);
  }
}

void WInPlaceEdit::render(WFlags<RenderFlag> flags)
{
  if (save_ && flags.test(RenderFlag::Full))
    wApp->theme()->apply(this, editing_, InPlaceEditing);

  WCompositeWidget::render(flags);
}
}
