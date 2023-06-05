/*
 * Copyright (C) 2011 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include "Login.h"

namespace Wt {
  namespace Auth {

Login::Login()
  : state_(LoginState::LoggedOut)
{ }

awaitable<void> Login::login(const User& user, LoginState state)
{
  if (state == LoginState::LoggedOut || !user.isValid()) {
    co_await logout();
    co_return;
  } else {
    if (state != LoginState::Disabled && user.status() == AccountStatus::Disabled)
      state = LoginState::Disabled;

    if (user != user_) {
      user_ = user;
      state_ = state;
      co_await changed_.emit();
    } else if (state != state_) {
      state_ = state;
      co_await changed_.emit();
    }
  }
  co_return;
}

awaitable<void> Login::logout()
{
  if (user_.isValid()) {
    user_ = User();
    state_ = LoginState::LoggedOut;
    co_await changed_.emit();
  }
  co_return;
}

LoginState Login::state() const
{
  return state_;
}

bool Login::loggedIn() const
{
  return user_.isValid() && state_ != LoginState::Disabled;
}

const User& Login::user() const
{
  return user_;
}

  }
}
