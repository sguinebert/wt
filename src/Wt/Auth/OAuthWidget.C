/*
 * Copyright (C) 2016 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include "Wt/Auth/OAuthService.h"
#include "Wt/Auth/OAuthWidget.h"

namespace Wt {
  namespace Auth {

OAuthWidget::OAuthWidget(const OAuthService& oAuthService)
  : WImage("css/oauth-" + oAuthService.name() + ".png")
{
  setToolTip(oAuthService.description());
  setStyleClass("Wt-auth-icon");
  setVerticalAlignment(AlignmentFlag::Middle);

  process_ = oAuthService.createProcess(oAuthService.authenticationScope());
#ifndef WT_TARGET_JAVA
  clicked().connect<&OAuthProcess::startAuthenticate>(process_.get());
#else
  auto clickedSignal = clicked();
  process_->connectStartAuthenticate(clickedSignal);
#endif

  process_->authenticated().connect<&OAuthWidget::oAuthDone>(this);
}

awaitable<void> OAuthWidget::oAuthDone(const Identity& identity)
{
  co_await authenticated_.emit(process_.get(), (Identity&)identity);
}

  }
}
