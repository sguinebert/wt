/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include "Wt/WApplication.h"
#include "Wt/WEnvironment.h"
#include "Wt/WTimer.h"
#include "Wt/WTimerWidget.h"
#include "Wt/WContainerWidget.h"
#include "TimeUtil.h"

#include "cuehttp/detail/engines.hpp"
#include <algorithm>

namespace Wt {

#warning "Need an implementation from pure client side timeout and one server side timeout"
WTimer::WTimer(bool clientSide)
  : uTimerWidget_(new WTimerWidget(this)),
    interval_(0),
    singleShot_(false),
    active_(false),
    clientSideTimeout_(clientSide),
    timeout_(new Time())
{
  timerWidget_ = uTimerWidget_.get();
  //timeout().connect<&WTimer::gotTimeout>(this); //not needed (if ajax is enabled & client side timeout) or handled by coroutine (if ajax is not enabled)
}

EventSignal<WMouseEvent>& WTimer::timeout()
{
  return timerWidget_->clicked();
}

WTimer::~WTimer()
{
  if (active_)
    stop();
}

void WTimer::setInterval(std::chrono::milliseconds msec)
{
  interval_ = msec;
}

void WTimer::setSingleShot(bool singleShot)
{
  singleShot_ = singleShot;
}

void WTimer::start()
{
    WApplication *app = WApplication::instance();
    if (!active_) {
        if (auto app = WApplication::instance(); app && app->timerRoot())
            app->timerRoot()->addWidget(std::move(uTimerWidget_));
    }
    active_ = true;
    *timeout_ = Time() + static_cast<int>(interval_.count());

    bool jsRepeat = !singleShot_ &&
                    ((app && app->environment().ajax()) ||
                     !timeout().isExposedSignal());

    timerWidget_->timerStart(jsRepeat);

    if(clientSideTimeout_ && app && app->environment().ajax())
        return;


    auto executor = http::detail::engines::thread_context;
    co_spawn(*executor, [&, interval =  static_cast<int>(interval_.count())]() -> awaitable<void> {
        auto executor = co_await asio::this_coro::executor;
        asio::steady_timer timer(executor);
        if(jsRepeat) {
            for(;;) {
                timer.expires_after(std::chrono::milliseconds(interval));
                co_await timer.async_wait(asio::bind_cancellation_slot(timer_cancel_.slot(), use_nothrow_awaitable));
                if(!active_)
                    break;
                //emit signal timeout
                co_await timeout().emit(WMouseEvent());
            }
        }
        else {
            timer.expires_after(std::chrono::milliseconds(interval));
            co_await timer.async_wait(asio::bind_cancellation_slot(timer_cancel_.slot(), use_nothrow_awaitable));
            //emit signal timeout
            if(active_)
                co_await timeout().emit(WMouseEvent());
        }
        stop();
    }, detached);
}

void WTimer::stop()
{
  if (active_) {
    if (timerWidget_ && timerWidget_->parent()) {
      uTimerWidget_ = timerWidget_->parent()->removeWidget(timerWidget_.get());
    }
    active_ = false;
    timer_cancel_.emit(asio::cancellation_type::total);
  }
}

//if the timer is set on client side, the repeated timer will be handled on client side
//if the timer is set on server side [by dev or because no ajax on user client], the repeated timer will be handled on server side via a coroutine
//this code is deprecated and should be removed
void WTimer::gotTimeout()
{
  if (active_) {
    if (!singleShot_) {
      *timeout_ = Time() + static_cast<int>(interval_.count());
      if (!timerWidget_->jsRepeat()) {
        WApplication *app = WApplication::instance();
        timerWidget_->timerStart(app->environment().ajax());
      }
    } else
      stop();
  }
}

int WTimer::getRemainingInterval() const
{
  int remaining = *timeout_ - Time();
  return std::max(0, remaining);
}

}
