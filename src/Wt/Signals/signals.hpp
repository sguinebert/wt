//// This may look like C code, but it's really -*- C++ -*-
///*
// * Copyright (C) 2016 Emweb bv, Herent, Belgium.
// *
// * See the LICENSE file for terms of use.
// */

//// Based on: https://testbit.eu/cpp11-signal-system-performance/
////   CC0 Public Domain: http://creativecommons.org/publicdomain/zero/1.0/

//#ifndef WT_SIGNALS_SIGNALS_HPP
//#define WT_SIGNALS_SIGNALS_HPP

//#include <Wt/Core/observable.hpp>
//#include <Wt/Core/observing_ptr.hpp>
//#include <Wt/WDllDefs.h>
//#include <Wt/AsioWrapper/asio.hpp>
//#include <Wt/cuehttp/detail/common.hpp>

//#include <cassert>
//#include <cstdint>
//#include <functional>
//#include <vector>

////extern thread_local asio::io_context* thread_context;

//namespace Wt { namespace Signals { namespace Impl {

//class Connection;

//class WT_API SignalLinkBase
//{
//public:
//  using CBUnlink = void (*)(SignalLinkBase*);

//  SignalLinkBase(CBUnlink unlink_callback);
//  ~SignalLinkBase();

//  SignalLinkBase(const SignalLinkBase&) = delete;
//  SignalLinkBase(SignalLinkBase&&) = delete;
//  SignalLinkBase& operator=(const SignalLinkBase&) = delete;
//  SignalLinkBase& operator=(SignalLinkBase&&) = delete;

//  Connection connect(const Wt::Core::observable *object);

//  void disconnect();
//  bool isConnected() const;

//protected:
//  bool connected_;
//  Wt::Core::observing_ptr<const Wt::Core::observable> obj_;
//  const Connection *connection_ring_;
//  CBUnlink unlink_callback_;

//  friend class Connection;
//};

//class WT_API Connection
//{
//public:
//  Connection() noexcept;
//  Connection(const Connection& conn) noexcept;
//  Connection(Connection&& conn) noexcept;
//  ~Connection() noexcept;

//  Connection& operator= (const Connection& other) noexcept;
//  Connection& operator= (Connection&& other) noexcept;
//  void disconnect() noexcept;
//  bool isConnected() const noexcept;

//private:
//  mutable const Connection *next_, *prev_;
//  mutable SignalLinkBase *signalLink_;

//  Connection(SignalLinkBase *signalLink) noexcept;
//  void clear() noexcept;
//  void unlinkAll() const noexcept;

//  friend class SignalLinkBase;
//};

//template<class... Args>
//class ProtoSignal
//{
//protected:
//  typedef std::function<awaitable<void> (Args...)> CbFunction;
//  typedef std::function<void (Args...)> syncFunction;

//private:
//  struct SignalLink : public SignalLinkBase {
//    SignalLink *next, *prev;
//    CbFunction function;
//    int ref_count;

//    static void unlinkBase(SignalLinkBase *self)
//    {
//      SignalLink *link = static_cast<SignalLink*>(self);
//      link->unlink();
//    }

//    SignalLink ()
//      : SignalLinkBase(&SignalLink::unlinkBase),
//        next (nullptr),
//    prev (nullptr),
//    ref_count (1) { }

//    ~SignalLink()
//    {
//      assert (ref_count == 0);
//    }

//    void incref()
//    {
//      ref_count += 1; assert (ref_count > 0);
//    }

//    void decref()
//    {
//      ref_count -= 1;
//      if (!ref_count)
//#pragma GCC diagnostic ignored "-Wfree-nonheap-object"
//        delete this;
//      else
//    assert (ref_count > 0);
//    }

//    void unlink(bool doDecref = true)
//    {
//      function = nullptr;
//      if (next)
//        next->prev = prev;
//      if (prev)
//        prev->next = next;
//      if (doDecref) {
//        decref();
//      }
//      // leave intact ->next, ->prev for stale iterators
//    }

//    void add_before(SignalLink *link)
//    {
//      link->prev = prev; // link to last
//      link->next = this;
//      prev->next = link; // link from last
//      prev = link;
//      static_assert (sizeof (link) == sizeof (size_t), "sizeof size_t");
//    }

//    Connection add_before (CbFunction &&cb, const Wt::Core::observable *object)
//    {
//      SignalLink *link = new SignalLink ();
//      add_before(link);
//      link->function = std::move(cb);
//      return link->connect(object);
//    }

//    bool active() const
//    {
//      return isConnected() && function != nullptr;
//    }
//  };

//  SignalLink   *callback_ring_; // linked ring of callback nodes

//  ProtoSignal (const ProtoSignal&) = delete;
//  ProtoSignal& operator= (const ProtoSignal&) = delete;

//  void ensure_ring ()
//  {
//    if (!callback_ring_) {
//      callback_ring_ = new SignalLink (); // ref_count = 1
//      callback_ring_->incref(); // ref_count = 2, head of ring, can be deactivated but not removed
//      callback_ring_->next = callback_ring_; // ring head initialization
//      callback_ring_->prev = callback_ring_; // ring tail initialization
//    }
//  }

//public:
//  // ProtoSignal constructor, connects default callback if non-nullptr.
//  ProtoSignal ()
//    : callback_ring_ (nullptr)
//  { }

//  // ProtoSignal destructor releases all resources associated with this signal.
//  ~ProtoSignal ()
//  {
//    if (callback_ring_) {
//      if (callback_ring_->ref_count == 2) {
//    while (callback_ring_->next != callback_ring_)
//      callback_ring_->next->unlink();
//      }
//      assert (callback_ring_->ref_count >= 2);
//      callback_ring_->decref();
//      callback_ring_->decref();
//    }
//  }

//  // Operator to add a new function or lambda as signal handler,
//  // returns a handler connection ID.

////  Connection connect(CbFunction &&cb,
////		     const Wt::Core::observable *object = nullptr) {
////    ensure_ring();
////    return callback_ring_->add_before(std::move(cb), object);
////  }
//  //wrap sync function/method
//  template<typename Cb>
//  Connection connect(Cb &&cb, const Wt::Core::observable *object = nullptr) {
//    ensure_ring();
//    if constexpr(http::detail::is_awaitable_function_v<Cb, Args...>) {
//      return callback_ring_->add_before(std::move(cb), object);
//    }
//    else { //call from Wt::Signals::connection Signal<A...>::connect(WObject *target, WObject::Method method)
//      CbFunction wrapper = [cb = std::move(cb)] (Args... args) -> awaitable<void> { cb(args...); co_return; };
//      return callback_ring_->add_before(std::move(wrapper), object);
//    }
//  }

//  bool isConnected() const
//  {
//    if (!callback_ring_)
//      return false;

//    SignalLink *link = callback_ring_;
   
//    do {
//      if (link->active())
//    return true;
//      link = link->next;
//    } while (link != callback_ring_);

//    return false;
//  }
//  // Emit a signal, i.e. invoke all its callbacks.
//  awaitable<void> emit (Args... args) const
//  {
//    if (!callback_ring_)
//      co_return;


//    // this ProtoSignal may be deleted in a callback,
//    // so take a defensive copy of the callback_ring pointer
//    SignalLink *const callback_ring = callback_ring_;

//    callback_ring->incref();
//    SignalLink *link = callback_ring_;
//    link->incref();

//    {
//      SignalLink end_link; // Marks the last slot to execute, any slots added while
//                           // executing this signal will not be executed during this
//                           // emit. If emit is called again while the signal is already
//                           // being emitted, those slots added before the second emit will
//                           // be executed.
//      callback_ring->add_before(&end_link);

//      do {
//        if (link->active()) {
//          try {
////            auto io_context = http::detail::engines::get_thread_context();
////            std::cout << "thread_context : " << io_context << std::endl;
////            co_spawn(*io_context, link->function(args...), asio::use_future).get();
//            co_await link->function(args...);
//          } catch (...) {
//            link->decref();

//            end_link.unlink(false);
//            assert(end_link.ref_count == 1);
//            end_link.ref_count = 0; // satisfy assertion in end_link destructor

//            if (callback_ring->ref_count < 2) {
//              assert(callback_ring->ref_count == 1);
//              while (callback_ring->next != callback_ring)
//                callback_ring->next->unlink();
//            }
//            callback_ring->decref();
//            throw;
//          }
//        }

//        SignalLink *old = link;
//        link = old->next;
//        if (link != &end_link)
//          link->incref();
//        old->decref();
//      } while (link != &end_link);

//      end_link.unlink(false);
//      assert(end_link.ref_count == 1);
//      end_link.ref_count = 0; // satisfy assertion in end_link destructor
//    }

//    if (callback_ring->ref_count < 2) {
//      assert(callback_ring->ref_count == 1);
//      while (callback_ring->next != callback_ring)
//    callback_ring->next->unlink();
//    }
//    callback_ring->decref();
//    co_return;
//  }
//};

//template <typename Function>
//struct function_traits : public function_traits<decltype(&Function::operator())>
//{};

//template <typename ClassType, typename ReturnType, typename... Args>
//struct function_traits<ReturnType(ClassType::*)(Args...) const>
//{
//  static constexpr int argCount = sizeof...(Args);
//  typedef std::function<ReturnType(Args...)> function;
//};

//template <typename ClassType, typename ReturnType, typename... Args>
//struct function_traits<ReturnType(ClassType::*)(Args...)>
//{
//  static constexpr int argCount = sizeof...(Args);
//  typedef std::function<ReturnType(Args...)> function;
//};

//template <typename Function>
//typename function_traits<Function>::function toFunction(Function&& lambda)
//{
//  return static_cast<typename function_traits<Function>::function>(std::move(lambda));
//}

//} // Impl

//typedef Impl::Connection connection;

//template <class... Args>
//struct Signal : Impl::ProtoSignal<Args...>
//{
//  typedef Impl::ProtoSignal<Args...> ProtoSignal;
//  typedef typename ProtoSignal::CbFunction CbFunction;

//  Signal() { }
//};

//namespace Impl
//{

//template <int Bcount, class... Args>
//struct ConnectHelper {
//  template <class T, class V, class... B>
//  static connection connect(Signal<Args...>& signal,
//                T *target, void (V::*method)(B...))
//  {
//    return signal.connect([target, method](Args... a) -> awaitable<void>
//                          {
//                            if constexpr(http::detail::is_awaitable_function_v<V, Args...>) {
//                               co_await (target ->* method) (a...);
//                            }
//                            else {
//                             (target ->* method) (a...);
//                            }
//                            co_return;
//                          }, target);
//  }

//  template <class T, class V, class... B>
//  static connection connect(Signal<Args...>& signal,
//                            T *target, awaitable<void> (V::*method)(B...))
//  {
//    return signal.connect([target, method](Args... a) -> awaitable<void>
//                          {
//                              co_await (target ->* method) (a...);
//                              co_return;
//                          }, target);
//  }
//};

//template <class... Args>
//struct ConnectHelper<0, Args...> {
//  static connection connect(Signals::Signal<Args...>& signal,
//                            const Core::observable *target,
//                            std::function<void ()>&& f)
//  {
//    return signal.connect([f WT_CXX14ONLY(=std::move(f))](Args...) -> awaitable<void> { f(); co_return; }, target);
//  }

//  static connection connect(Signals::Signal<Args...>& signal,
//                            const Core::observable *target,
//                            std::function<awaitable<void> ()>&& f)
//  {
//    return signal.connect([f WT_CXX14ONLY(=std::move(f))](Args...) -> awaitable<void> { co_await f(); co_return; }, target);
//  }

////  static connection connect(Signals::Signal<Args...>& signal,
////                            const Core::observable *target,
////                            std::function<awaitable<void> ()>&& f)
////  {
////    return signal.connect(std::move(f), target);
////  }

//  template <class T, class V>
//  static connection connect(Signals::Signal<Args...>& signal, T *target, void (V::*method)())
//  {
//    return signal.connect([target, method](Args...) -> awaitable<void> {
//        if constexpr(http::detail::is_awaitable_function_v<V, Args...>) {
//            co_await (target ->* method) ();
//        }
//        else {
//            (target ->* method) ();
//        }
//        co_return;
//    }, target);
//  }

//  template <class T, class V>
//  static connection connect(Signals::Signal<Args...>& signal,
//                            T *target, awaitable<void> (V::*method)())
//  {
//    return signal.connect([target, method](Args...) -> awaitable<void> {
//        co_await (target ->* method) ();
//        co_return;
//    }, target);
//  }
//};

//template <class... Args>
//struct ConnectHelper<1, Args...> {
//  template <typename B1, typename... An>
//  static connection connect(Signals::Signal<Args...>& signal,
//                const Core::observable *target,
//                            std::function<void (B1)>&& f)
//  {
//    return signal.connect([f WT_CXX14ONLY(=std::move(f))](B1 b1, An...) -> awaitable<void> { f(b1); co_return; }, target);
//  }

//  template <typename B1, typename... An>
//  static connection connect(Signals::Signal<Args...>& signal,
//                            const Core::observable *target,
//                            std::function<awaitable<void> (B1)>&& f)
//  {
//    return signal.connect([f WT_CXX14ONLY(=std::move(f))](B1 b1, An...) -> awaitable<void> { co_await f(b1); co_return; }, target);
//  }

//  template <class T, class V, typename B1, typename... An>
//  static connection connect(Signals::Signal<Args...>& signal, T *target, void (V::*method)(B1))
//  {
//    return signal.connect
//        ([target, method](B1 b1, An...) -> awaitable<void> {
//            (target ->* method) (b1);
//            co_return;
//        }, target);
//  }

//  template <class T, class V, typename B1, typename... An>
//  static connection connect(Signals::Signal<Args...>& signal, T *target, awaitable<void> (V::*method)(B1))
//  {
//    return signal.connect
//        ([target, method](B1 b1, An...) -> awaitable<void> {
//            co_await (target ->* method) (b1);
//            co_return;
//        }, target);
//  }
//};

//template <class... Args>
//struct ConnectHelper<2, Args...> {
//  template <typename B1, typename B2, typename... An>
//  static connection connect(Signals::Signal<Args...>& signal,
//                const Core::observable *target,
//                            std::function<void (B1, B2)>&& f)
//  {
//    return signal.connect([f WT_CXX14ONLY(=std::move(f))](B1 b1, B2 b2, An...) -> awaitable<void> { f(b1, b2); co_return; }, target);
//  }

//  template <typename B1, typename B2, typename... An>
//  static connection connect(Signals::Signal<Args...>& signal,
//                            const Core::observable *target,
//                            std::function<awaitable<void> (B1, B2)>&& f)
//  {
//    return signal.connect([f WT_CXX14ONLY(=std::move(f))](B1 b1, B2 b2, An...) -> awaitable<void> { co_await f(b1, b2); co_return; }, target);
//  }

//  template <class T, class V, typename B1, typename B2, typename... An>
//  static Wt::Signals::connection connect(Signals::Signal<Args...>& signal,
//                     T *target, void (V::*method)(B1, B2))
//  {
//    return signal.connect
//        ([target, method](B1 b1, B2 b2, An...) -> awaitable<void> {
//            (target ->* method) (b1, b2);
//            co_return;
//        }, target);
//  }

//  template <class T, class V, typename B1, typename B2, typename... An>
//  static Wt::Signals::connection connect(Signals::Signal<Args...>& signal,
//                                         T *target, awaitable<void> (V::*method)(B1, B2))
//  {
//    return signal.connect
//        ([target, method](B1 b1, B2 b2, An...) -> awaitable<void> {
//            co_await (target ->* method) (b1, b2);
//            co_return;
//        }, target);
//  }
//};

//template <class... Args>
//struct ConnectHelper<3, Args...> {
//  template <typename B1, typename B2, typename B3, typename... An>
//  static connection connect(Signals::Signal<Args...>& signal,
//                const Core::observable *target,
//                            std::function<void (B1, B2, B3)>&& f)
//  {
//    return signal.connect([f WT_CXX14ONLY(=std::move(f))](B1 b1, B2 b2, B3 b3, An...) -> awaitable<void> {
//        f(b1, b2, b3);
//        co_return;
//    }, target);
//  }

//  template <typename B1, typename B2, typename B3, typename... An>
//  static connection connect(Signals::Signal<Args...>& signal,
//                            const Core::observable *target,
//                            std::function<awaitable<void> (B1, B2, B3)>&& f)
//  {
//    return signal.connect([f WT_CXX14ONLY(=std::move(f))](B1 b1, B2 b2, B3 b3, An...) -> awaitable<void> {
//        co_await f(b1, b2, b3);
//        co_return;
//    }, target);
//  }

//  template <class T, class V, typename B1, typename B2, typename B3, typename... An>
//  static Wt::Signals::connection connect
//     (Signals::Signal<Args...>& signal,
//      T *target, void (V::*method)(B1, B2, B3))
//  {
//    return signal.connect
//        ([target, method](B1 b1, B2 b2, B3 b3, An...) -> awaitable<void> {
//            (target ->* method) (b1, b2, b3);
//            co_return;
//        }, target);
//  }

//  template <class T, class V, typename B1, typename B2, typename B3, typename... An>
//  static Wt::Signals::connection connect
//      (Signals::Signal<Args...>& signal,
//       T *target, awaitable<void> (V::*method)(B1, B2, B3))
//  {
//    return signal.connect
//        ([target, method](B1 b1, B2 b2, B3 b3, An...) -> awaitable<void> {
//            co_await (target ->* method) (b1, b2, b3);
//            co_return;
//        }, target);
//  }
//};

///* TODO: for 4...10 ? */
//template <typename F, class... Args>
//connection connectFunction
//(Signal<Args...>& signal,
// typename std::enable_if<!std::is_bind_expression<F>::value, F&&>::type function,
// const Core::observable *target)
//{
////  if constexpr(http::detail::is_awaitable_lambda_v<F>) {
//    return ConnectHelper<function_traits<F>::argCount, Args...>
//        ::connect(signal, target, Signals::Impl::toFunction(std::move(function)));
////  }
////  else {
////    return ConnectHelper<function_traits<F>::argCount, Args...>
////        ::connect(signal, target, std::move(function));
////  }
////  else {
////    auto lambda = [function = std::move(function)] (Args... args) -> awaitable<void> { function(args...); co_return; };
////    return ConnectHelper<function_traits<F>::argCount, Args...>
////        ::connect(signal, target, Signals::Impl::toFunction(std::move(lambda)));
////  }

//}

//template <typename F, class... Args>
//connection connectFunction
//(Signal<Args...>& signal,
// typename std::enable_if<std::is_bind_expression<F>::value, F&&>::type function,
// const Core::observable *target)
//{
////  if constexpr(http::detail::is_awaitable_function<F, Args...>::value) {
//    return signal.connect(std::move(function), target);
////  }
////  else {
////    auto lambda = [function = std::move(function)] (Args... args) -> awaitable<void> { function(args...); co_return; };
////    return signal.connect(Signals::Impl::toFunction(std::move(lambda)), target);
////  }
//}

//}

//}}

//#endif // __SIMPLE_SIGNAL_HH__
