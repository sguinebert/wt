// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#ifndef WSIGNAL_H_
#define WSIGNAL_H_

#include <Wt/WObject.h>
#include <bitset>

#ifndef WT_TARGET_JAVA
#include <Wt/Signals/signals.hpp> //DEPRECATED
#include <Wt/Signals/nano_signal_slot.hpp>
#endif // WT_TARGET_JAVA

#ifdef WT_THREADED
#include <atomic>
#endif // WT_THREADED

#include <functional>
#include <Wt/AsioWrapper/asio.hpp>

namespace Wt {

class JSlot;
class SlotLearnerInterface;
class WStatelessSlot;
class JavaScriptEvent;

struct NoClass
{
#ifndef WT_TARGET_JAVA
  constexpr
#endif // WT_TARGET_JAVA
    NoClass() { }
  NoClass(const JavaScriptEvent&) { }

#ifdef WT_TARGET_JAVA
  static const NoClass none;
#endif // WT_TARGET_JAVA
};

#ifdef WT_TARGET_JAVA
namespace Signals {
  struct connection {
    connection();
    connection(const connection &);

    void disconnect();
    bool isConnected() const;
  };
}
#endif // WT_TARGET_JAVA

/*! \defgroup signalslot Signal/slot system
    \brief Support for event handling using signals and slots.

   To respond to user-interactivity events, or in general to communicate
   events from one widget to any other, %Wt uses a signal/slot system.
  
   A slot is any method of any descendant of WObject. To connect a
   signal with a slot, the only requirement is that the method
   signature of the slot must be compatible with the signal
   definition. In this way every method may be used as a slot, and it
   is not necessary to explicitly indicate a particular method to be a
   slot (as is needed in Qt), by putting them in a special section.
   Nevertheless, you may still do that if you wish to emphasize that
   these functions can be used as slots, or, if you have done extra
   work to optimize the implementation of these methods as client-side
   JavaScript code (see below).
  
   A signal may be created by adding a \link Signal Signal<X,
   ...>\endlink object to your class. You may specify up to 6
   arguments which may be of arbitrary types that are <i>Copyable</i>,
   that may be passed through the signal to connected slots.
  
   The library defines several user-event signals on various widgets,
   and it is easy and convenient to add signals and slots to widget
   classes to communicate events and trigger callbacks.
  
   Event signals (\link EventSignal EventSignal<E>\endlink)
   are signals that may be triggered internally by the library to
   respond to user interactivity events.  The abstract base classes
   WInteractWidget and WFormWidget define most of these event
   signals. To react to one of these events, the programmer connects a
   self-defined or already existing slot to such a signal.

   To connect a signal from multiple senders to a single slot, we
   recommend the use of std::bind() to identify the sender (or
   otherwise the intention) of the signal.

   \if cpp
   Usage example:
   \code
   std::vector<Wt::WPushButton*> buttons = ...;
   for(unsigned i = 0; i < buttons.size(); ++i) {
     buttons[i]->clicked().connect(std::bind(&Keyboard::handleClick, i));
   }

   void Keyboard::handleClick(int i) {
     t->setText(WString("You pressed button {1}").args(i));
   }
   \endcode
   \endif
*/

/*! \brief Abstract base class of a signal.
 *
 * Base class for all signals.
 *
 * \ingroup signalslot
 */
class WT_API SignalBase
{
public:
  virtual ~SignalBase();

  /*! \brief Returns whether this signal is connected.
   *
   * Returns true when the signal was connected to to at least one slot.
   */
  virtual bool isConnected() const = 0;

  /*! \brief Connects to a slot.
   *
   * Every signal can be connected to a slot which does not take any
   * arguments (and may thus ignore signal arguments).
   */
//  virtual Wt::Signals::connection connect(WObject *target, WObject::Method method) = 0;

//  virtual Wt::Signals::connection connect(WObject *target, WObject::AsyncMethod method) = 0;

  /*! \brief Connects to a slot.
   *
   * Every signal can be connected to a slot which does not take any
   * arguments (and may thus ignore signal arguments).
   */
  template<class T, class V>
  Wt::Signals::connection connect(T *target, void (V::*method)())
  {
      WObject *o = dynamic_cast<WObject *>(dynamic_cast<V *>(target));
      assert(o);
      return connect(o, static_cast<WObject::Method>(method));
  }

  template<class T, class V>
  Wt::Signals::connection connect(T *target, awaitable<void> (V::*method)())
  {
      WObject *o = dynamic_cast<WObject *>(dynamic_cast<V *>(target));
      assert(o);
      return connect(o, static_cast<WObject::AsyncMethod>(method));
  }


protected:
  SignalBase();

private:
  SignalBase(const SignalBase& other);
};

/*
 * Normal signals wrap simply around boost signals
 */

/*! \class Signal Wt/WSignal.h Wt/WSignal.h
 *  \brief A signal that propagates events to listeners.
 *
 * Use Signal/slots to let one object (<i>A</i>) listen to events
 * caused by another object (<i>B</i>). In this scenario, object
 * <i>B</i> provides in its public interface access to a signal, to
 * which object <i>A</i> connects one of its member function (which
 * act as slot). Object <i>A</i> can then signal an event (which
 * triggers the connected callback functions), by emitting the
 * signal. Signal/slot is a generalization of the popular observer
 * pattern used in GUIs.
 *
 * A signal can provide details of the event, using up to 6
 * parameters. A slot must have a compatible signature to connect to
 * a signal, based on its parameters. A compatible signature provides
 * the same parameters in the member function, or less (leaving out
 * parameters at the end).
 *
 * The signal automatically disconnects from the slot when the
 * target is deleted. In addition, the signal may be deleted at any
 * time, in particular also while it is being emitted.
 *
 * \if cpp
 * \code
 * class MyWidget : public Wt::WContainerWidget
 * {
 * public:
 *   MyWidget()
 *     : Wt::WContainerWidget()
 *       done_(this)
 *   {
 *      ...
 *      Wt::WPushButton *button = addWidget(std::make_unique<Wt::WPushButton>("Okay"));
 *      button->clicked().connect(this, &MyWidget::process);
 *   }
 *
 *   // provide an accessor for the signal
 *   Wt::Signal<int, std::string>& done() { return done_; }
 *
 * private:
 *   Wt::Signal<int, std::string> done_;
 *
 *   void process() {
 *     ...
 *     done_.emit(42, "Totally done"); // emit the signal
 *   }
 * };
 * \endcode
 * \endif
 *
 * This widget could then be used from another class:
 * \if cpp
 * \code
 * class GUIClass : public Wt::WContainerWidget
 * {
 *   ...
 *
 * private:
 *   void init() {
 *     MyWidget *widget = addWidget(std::make_unique<MyWidget>());
 *     widget->done().connect(this, &GUIClass::whenDone);
 *   }
 *
 *   void whenDone(int result, const std::string& description) {
 *     ...
 *   }
 * };
 * \endcode
 * \endif
 *
 * \ingroup signalslot
 */

/* Creating aliases when using nano-signal-slot will increase the maintainability of your code
 * especially if you are choosing to use the alternative policies.*/
using NanoPolicy = ::Nano::ST_Policy_Safe;

template <class... A>
using Signal = ::Nano::Signal<A...> ;

using Observer = ::Nano::Observer<NanoPolicy>;


/*! \brief Abstract base class of an event signal.
 *
 * An event signal is directly associated with a user-interface generated
 * event.
 *
 * \ingroup signalslot
 */
class WT_API EventSignalBase : public SignalBase
{
public:
  virtual ~EventSignalBase();

  /*! \brief Returns the event name.
   *
   * The event name is implementation-defined.
   */
  const char *name() const { return name_; }

  bool needsUpdate(bool all) const;
  void updateOk();
  virtual bool isConnected() const override;

  virtual const std::string encodeCmd() const;
  const std::string javaScript() const;
  bool isExposedSignal() const;
  bool canAutoLearn() const;
  void setNotExposed();

  void disconnect(/*Wt::Signals::connection& conn*/);


  /*! \brief Disconnects a JSlot.
   */
  void disconnect(JSlot& slot);

  /*! \brief Prevents the default browser action.
   *
   * Use this method to prevent the default browser action associated
   * with this event.
   *
   * \sa setPreventPropagation()
   */
  void preventDefaultAction(bool prevent = true);

  /*! \brief Returns whether the default browser action is prevented.
   *
   * \sa preventDefaultAction()
   */
  bool defaultActionPrevented() const;

  /*! \brief Prevents event propagation.
   *
   * Use this method to prevent that the event propagates further to
   * its ancestors.
   *
   * \sa preventDefaultAction()
   */
  void preventPropagation(bool prevent = true);

  /*! \brief Returns whether event propagation is prevented.
   *
   * \sa preventPropagation()
   */
  bool propagationPrevented() const;

  const std::string createUserEventCall(const std::string& jsObject,
                                        const std::string& jsEvent,
                                        const std::string& eventName,
                                        std::initializer_list<std::string> args) const;

  Wt::Signals::connection connectStateless(WObject::Method method,
                                           WObject *target,
                                           WStatelessSlot *slot);
  using SignalBase::connect;
  void connect(JSlot& slot);
  void connect(const std::string& function);

  WObject *owner() const { return owner_; }
  void ownerRepaint();

protected:
  struct StatelessConnection {
    Wt::Signals::connection connection;
    WObject *target;
    WStatelessSlot *slot;

    bool ok() const;

    StatelessConnection(const Wt::Signals::connection& c, WObject *target, WStatelessSlot *slot);
  };

  static const int BIT_NEED_UPDATE = 0;
  static const int BIT_SERVER_EVENT = 1;
  static const int BIT_EXPOSED = 2;
  static const int BIT_CAN_AUTOLEARN = 3;
  static const int BIT_PREVENT_DEFAULT = 4;
  static const int BIT_PREVENT_PROPAGATION = 5;
  static const int BIT_SIGNAL_SERVER_ANYWAY = 6;

#ifdef WT_THREADED
  static std::atomic<unsigned> nextId_;
#else
  static unsigned nextId_;
#endif // WT_THREADED

  const char *name_;
  WObject *owner_;
  const unsigned id_;
  std::vector<StatelessConnection> connections_;
  std::bitset<7> flags_;

  /*
   * Dummy signal used for knowing if stateless connections are still
   * connected.
   */
  //Signals::Signal<> dummy_;
  Signal<void()> dummy_;


  EventSignalBase(const char *name, WObject *owner, bool autoLearn);

  void prepareDestruct();
  void exposeSignal();
  void processLearnedStateless() const;
  void processNonLearnedStateless() const;
  virtual int argumentCount() const = 0;

  static void *alloc();
  static void free(void *s);

private:
  /*
   * Our own list of connections to process them in a custom way.
   */
  void removeSlot(WStatelessSlot *slot);

  void processPreLearnStateless(SlotLearnerInterface *learner);
  void processAutoLearnStateless(SlotLearnerInterface *learner);
  virtual awaitable<void> processDynamic(const JavaScriptEvent& e) const = 0;

  friend class WStatelessSlot;
  friend class WebRenderer;
  friend class WebSession;
};

/*! \class EventSignal Wt/WSignal.h Wt/WSignal.h
 *  \brief A signal that conveys user-interface events.
 *
 * An %EventSignal is a special %Signal that may be triggered by user
 * interface events such as a mouse click, key press, or focus change.
 * They are made available through the library in widgets like
 * WInteractWidget, and should not be instantiated directly.
 *
 * In addition to the behaviour of %Signal, they are capable of both
 * executing client-side and server-side slot code. They may learn
 * JavaScript from C++ code, through stateless slot learning, when
 * connected to a slot that has a stateless implementation, using
 * WObject::implementStateless(). Or they may be connected to a JSlot
 * which provides manual JavaScript code.
 *
 * The typically relay UI event details, using event details objects
 * like WKeyEvent or WMouseEvent.
 *
 * \sa Signal, JSignal
 *
 * \ingroup signalslot
 */
template<typename E = NoClass>
class EventSignal : public EventSignalBase
{
public:
#ifndef WT_TARGET_JAVA
  static void *operator new(std::size_t size){
    return EventSignalBase::alloc();
  }
  static void operator delete(void *deletable, std::size_t size){
    EventSignalBase::free(deletable);
  }

  EventSignal(const char *name, WObject *owner): EventSignalBase(name, owner, true)
  { }
#else
  EventSignal(const char *name, WObject *object, const E& e);
#endif // WT_TARGET_JAVA

  /*! \brief Returns whether the signal is connected.
   */
  virtual bool isConnected() const override
  {
    if (EventSignalBase::isConnected())
        return true;

    return dynamic_.isConnected();
  }

  void disconnect(JSlot& slot)
  {
    EventSignalBase::disconnect(slot);
  }


  template<auto memptr, class T>
  void disconnect(T *target)
  {
    dynamic_. template disconnect<memptr>(target);
    EventSignalBase::disconnect();
  }

  /*! \brief Connects to a function.
   *
   * This variant of the overloaded connect() method supports a
   * template function object (which supports operator ()).
   *
   * When the receiver function is an object method, the signal will
   * automatically be disconnected when the object is deleted, as long as the
   * object inherits from WObject (or Wt::Signals::trackable).
   */
//  template <class F>
//  Wt::Signals::connection connect(F function)
//  {
//    exposeSignal();
//     return Signals::Impl::connectFunction<F, E>(dynamic_, std::move(function), nullptr);
//  }

//  template <class F>
//  Wt::Signals::connection connect(const WObject *target, F function)
//  {
//    exposeSignal();
//    return Signals::Impl::connectFunction<F, E>(dynamic_, std::move(function), target);
//  }

  template <class F>
  Wt::Signals::connection connect(F function)
  {
     exposeSignal();
     if constexpr(std::is_function_v<F>) {
        dynamic_. template connect<&function>();
     }
     else {
        dynamic_.connect(function);
     }
     return Wt::Signals::connection();
  }

  template <class F>
  Wt::Signals::connection connect(const WObject *target, F function)
  {
    exposeSignal();

     if constexpr(std::is_function_v<F>) {
        dynamic_. template connect<&function>(target);
     }
     else {
        dynamic_.connect(function);
     }

    return Wt::Signals::connection();
    //return Signals::Impl::connectFunction<F, E>(dynamic_, std::move(function), target);
  }

  /*! \brief Connects a slot that takes no arguments.
   *
   * If a stateless implementation is specified for the slot, then
   * the visual behaviour will be learned in terms of JavaScript, and
   * will be cached on the client side for instant feed-back, in
   * addition running the slot on the server.
   *
   * The slot is as a \p method of an object \p target of class \p T,
   * which equals class \p V, or is a base class of class \p V. In
   * addition, to check for stateless implementations, class \p T must
   * be also be a descendant of WObject. Thus, the following statement
   * must return a non-null pointer:
   *
   * \code
   * WObject *o = dynamic_cast<WObject *>(dynamic_cast<V *>(target));
   * \endcode
   */

  template<auto memptr, class T>
  void connect(T *target)
  {
    exposeSignal();

    WObject *o = dynamic_cast<WObject *>(target);
    assert(o);

    if constexpr (std::is_convertible_v<decltype(memptr), WObject::Method>)
    {
        WStatelessSlot *s = o->isStateless(static_cast<WObject::Method>(memptr));

        if (s) {
            dummy_. template connect<memptr>(target);
            EventSignalBase::connectStateless(static_cast<WObject::Method>(memptr), o, s);
            return;
        }
    }

    dynamic_. template connect<memptr>(target);
  }
//
//  template <class T, class V>
//  Wt::Signals::connection connect(T *target, void (V::*method)())
//  {
//    exposeSignal();
//    WObject *o = dynamic_cast<WObject *>(dynamic_cast<V *>(target));
//    assert(o);

//    WStatelessSlot *s = o->isStateless(static_cast<WObject::Method>(method));

//    if (s)
//      return EventSignalBase::connectStateless(static_cast<WObject::Method>(method), o, s);


//  #ifdef DYN_TEST
//    dynamic_. template connect<method>(target);
//    return Wt::Signals::connection();
//  #else
//    return dynamic_.connect(std::bind(method, target), o);
//  #endif
//  }

//  template <class T, class V>
//  Wt::Signals::connection connect(T *target, awaitable<void> (V::*method)())
//  {
//    exposeSignal();
//    WObject *o = dynamic_cast<WObject *>(dynamic_cast<V *>(target));
//    assert(o);

//    WStatelessSlot *s = o->isStateless(static_cast<WObject::Method>(method));

//    if (s)
//      return EventSignalBase::connectStateless(static_cast<WObject::Method>(method), o, s);

//  #ifdef DYN_TEST
//    dynamic_. template connect<method>(target);
//    return Wt::Signals::connection();
//  #else
//    return dynamic_.connect(std::bind(method, target), o);
//  #endif
//  }
//  /*! \brief Connects a slot that takes one argument.
//   *
//   * This is only possible for signals that take at least one argument.
//   */
//  template <class T, class V>
//  Wt::Signals::connection connect(T *target, void (V::*method)(E))
//  {
//    exposeSignal();
//    assert(dynamic_cast<V *>(target));

//  #ifdef DYN_TEST
//    dynamic_.connect(std::bind(method, target, std::placeholders::_1));
//    return Wt::Signals::connection();
//  #else
//    return dynamic_.connect(std::bind(method, target, std::placeholders::_1), target);
//  #endif
//  }

//  template <class T, class V>
//  Wt::Signals::connection connect(T *target, awaitable<void> (V::*method)(E))
//  {
//    exposeSignal();
//    assert(dynamic_cast<V *>(target));

//  #ifdef DYN_TEST
//    dynamic_.connect(std::bind(method, target, std::placeholders::_1));
//    return Wt::Signals::connection();
//  #else
//    return dynamic_.connect(std::bind(method, target, std::placeholders::_1), target);
//  #endif
//  }
//  /*! \brief Connects a slot that takes a 'const argument&'.
//   *
//   * This is only possible for signals that take at least one argument.
//   */
//  template <class T, class V>
//  Wt::Signals::connection connect(T *target, void (V::*method)(const E&))
//  {
//    exposeSignal();
//    assert(dynamic_cast<V *>(target));

//  #ifdef DYN_TEST
//    dynamic_.connect(std::bind(method, target, std::placeholders::_1));
//    return Wt::Signals::connection();
//  #else
//    return dynamic_.connect(std::bind(method, target, std::placeholders::_1), target);
//  #endif
//  }

//  template <class T, class V>
//  Wt::Signals::connection connect(T *target, awaitable<void> (V::*method)(const E&))
//  {
//    exposeSignal();
//    assert(dynamic_cast<V *>(target));

//  #ifdef DYN_TEST
//    dynamic_.connect(std::bind(method, target, std::placeholders::_1));
//    return Wt::Signals::connection();
//  #else
//    return dynamic_.connect(std::bind(method, target, std::placeholders::_1), target);
//  #endif
//  }


  /*! \brief Connects a JavaScript function.
   *
   * This will provide a client-side connection between the event and
   * a JavaScript function. The argument must be a JavaScript function
   * which optionally accepts two arguments (object and the event):
   *
   * \code
   * function(object, event) {
   *   ...
   * }
   * \endcode
   *
   * Unlike a JSlot, there is no automatic connection management: the
   * connection cannot be removed. If you need automatic connection
   * management, you should use connect(JSlot&) instead.
   */
  void connect(const std::string& function)
  {
    EventSignalBase::connect(function);
  }
  void connect(const char * function)
  {
    EventSignalBase::connect(function);
  }

  /*! \brief Connects a slot that is specified as JavaScript only.
   *
   * This will provide a client-side connection between the event and
   * some JavaScript code as implemented by the slot. Unlike other
   * connects, this does not cause the event to propagated to the
   * application, and thus the state changes caused by the JavaScript
   * slot are not tracked client-side.
   *
   * The connection is tracked, taking into account the life-time of
   * the JSlot object, and can be updated by modifying the \p slot. If
   * you do not need connection management (e.g. because the slot has
   * the same life-time as the signal), then you can use connect(const
   * std::string&) instead.
   */
  void connect(JSlot& slot)
  {
    EventSignalBase::connect(slot);
  }

  /*! \brief Emits the signal.
   *
   * This will cause all connected slots to be triggered, with the given
   * argument.
   */
  awaitable<void> emit(E e = NoClass()) const
  {
    processLearnedStateless();
    processNonLearnedStateless();

    if constexpr(std::is_same_v<E, NoClass>)
        co_await dynamic_.emit();
    else
        co_await dynamic_.emit(e);
  }

  /*! \brief Emits the signal.
   *
   * This is equivalent to emit().
   *
   * \sa emit()
   */
  awaitable<void> operator()(E e) const
  {
    co_await emit(e);
  }

//#define DYN_TEST
//  virtual Wt::Signals::connection connect(WObject *target, WObject::Method method) override
//  {
//    exposeSignal();

//    WStatelessSlot *s = target->isStateless(method);
//    if (s)
//        return EventSignalBase::connectStateless(method, target, s);

//#ifdef DYN_TEST
//        //dynamic_. template connect<WObject::AsyncMethod>(target);
//    dynamic_.connect(std::bind(method, target));
//    return Wt::Signals::connection();
//#else
//    return dynamic_.connect(std::bind(method, target), target);
//#endif
//  }


//  virtual Wt::Signals::connection connect(WObject *target, WObject::AsyncMethod method) override
//  {
//    exposeSignal();

//#ifdef DYN_TEST
//        //dynamic_. template connect<WObject::AsyncMethod>(target);
//    dynamic_.connect(std::bind(method, target));
//    return Wt::Signals::connection();
//#else
//    return dynamic_.connect(std::bind(method, target), target);
//#endif
//  }

protected:
  virtual int argumentCount() const override
  {
    return 0; // excluding the event 'e'
  }

private:
//#ifdef DYN_TEST
  using SignalType = std::conditional_t<std::is_same_v<E, NoClass>, Signal<awaitable<void>()>, Signal<awaitable<void>(E)>>;
//#else
//  typedef Signals::Signal<E> SignalType;
//#endif
  SignalType dynamic_;
//  std::conditional_t<std::is_same_v<E, NoClass>, Signal<awaitable<void>()>, Signal<awaitable<void>(E)>> test_;


  awaitable<void> processDynamic(const JavaScriptEvent& jse) const override
  {
    processNonLearnedStateless();

    E event(jse);

    if constexpr(std::is_same_v<E, NoClass>)
        co_await dynamic_.emit();
    else
        co_await dynamic_.emit(event);
    co_return;
  }
};

}

#endif // WSIGNAL_H_
