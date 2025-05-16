#pragma once

#include "nano_function.hpp"
#include "nano_observer.hpp"
#include <iostream>

namespace Nano
{

template <typename RT, typename MT_Policy = ST_Policy>
class Signal;
template <typename RT, typename MT_Policy, typename... Args>
class Signal<RT(Args...), MT_Policy> final : public Observer<MT_Policy>
{
    using observer = Observer<MT_Policy>;
    using function = Function<RT(Args...)>;

    template <typename T>
    auto insert_sfinae(Delegate_Key const& key, typename T::Observer* instance) -> observer::Connection&
    {
        observer::insert(key, instance);
        return instance->insert(key, this);
    }
    template <typename T>
    void remove_sfinae(Delegate_Key const& key, typename T::Observer* instance)
    {
        observer::remove(key);
        instance->remove(key);
    }
    template <typename T>
    auto insert_sfinae(Delegate_Key const& key, ...) -> observer::Connection&
    {
        return observer::insert(key, this);
    }
    template <typename T>
    void remove_sfinae(Delegate_Key const& key, ...)
    {
        observer::remove(key);
    }

    public:

    Signal() noexcept = default;
    ~Signal() noexcept = default;

    Signal(Signal const&) noexcept = delete;
    Signal& operator= (Signal const&) noexcept = delete;

    Signal(Signal&&) noexcept = default;
    Signal& operator=(Signal&&) noexcept = default;

    template <auto mem_ptr, typename T>
    Observer<MT_Policy>::Connection make_Connection(T* instance)
    {
        Delegate_Key key = function::template bind<mem_ptr>(instance);
        return typename Observer<MT_Policy>::Connection(key, this);
    }
    template <typename L>
    Observer<MT_Policy>::Connection make_Connection(L* function)
    {
        Delegate_Key key = function::template bind<L>(function);
        return typename Observer<MT_Policy>::Connection(key, this);
    }

    //-------------------------------------------------------------------CONNECT

    template <typename L>
    auto connect(L* instance) -> observer::Connection&
    {
        return observer::insert(function::template bind<L>(instance), this);
    }
    /* connect to a lambda or std::bind callable object passed by r or l-value */
    template <typename L>
    auto connect(L&& callable) -> observer::Connection&
    {
        using f_type = std::decay_t<L>;
        using function_pointer_type = RT(*)(Args...);

        /* Case 1: L is an lvalue reference (e.g., an existing lambda variable) */
        if constexpr(std::is_lvalue_reference_v<L>) {
            // Store a raw pointer to the existing lvalue functor.
            // User is responsible for the lifetime of 'instance'.
            return connect(std::addressof(callable)); // Calls connect(L* instance)
        }
        /* Case 2: L is an rvalue, and it's a stateless lambda (convertible to function pointer) */
        else if constexpr (std::is_convertible_v<f_type, function_pointer_type>) {
            // The lambda is stateless. We can obtain a function pointer to it.
            // The unary '+' operator on a stateless lambda decays it to a function pointer.
            // This function pointer is then passed to the connect overload for static functions.
            // No heap allocation needed for the lambda itself.
            auto func_ptr = +callable; // Get the function pointer
            return connect(func_ptr);      // Call the connect overload for static function pointers
        }
        /* Case 3: L is an rvalue and is stateful (or not a lambda convertible to func ptr) */
        else {
            // Allocate & copy/move the stateful rvalue functor on the heap.
            // Keep shared_ptr alive in the observer's storage.
            auto heap = std::make_shared<f_type>(std::move(callable));
            auto func_ptr = heap.get(); // Get the function pointer
            return observer::insert(function::template bind<f_type>(func_ptr), this, std::move(heap));
        }
    }
    /* Why ?  lifetime monitor: the slot will auto-disconnect
     *                   when *target* is destroyed (if we pass std::bind(..., this, c) */
    template <typename L, typename T>
    auto connect(T* instance, L&& callable) -> observer::Connection&
    {
        using f_type = std::decay_t<L>;
        using function_pointer_type = RT(*)(Args...);
        /* it is a reference to a functor (example a lambda passed by ref) - you need to watch the lifetime of the lambda */
        if constexpr(std::is_lvalue_reference_v<L>) {
            return insert_sfinae<T>(function::template bind<std::addressof(callable)>(instance), instance);
        }
        /* Case 2: L is an rvalue, and it's a stateless lambda (convertible to function pointer) */
        else if constexpr (std::is_convertible_v<f_type, function_pointer_type>) {
            // The lambda is stateless. We can obtain a function pointer to it.
            // The unary '+' operator on a stateless lambda decays it to a function pointer.
            // This function pointer is then passed to the connect overload for static functions.
            // No heap allocation needed for the lambda itself.
            auto func_ptr = +callable; // Get the function pointer
            return insert_sfinae<T>(function::template bind<std::addressof(func_ptr)>(instance), instance);
        }
        /* Case 3: L is an rvalue and is stateful (or not a lambda convertible to func ptr) */
        else {
            using f_type = std::remove_pointer_t<std::remove_reference_t<L>>;
            // Allocate & copy/move the stateful rvalue functor on the heap.
            // Keep shared_ptr alive in the observer's storage.
            auto heap = std::make_shared<f_type>(std::move(callable));
            auto func_ptr = heap.get(); // Get the function pointer
            return observer::insert(function::template bind<f_type>(func_ptr), instance, std::move(heap));
        }
    }

    /* static function connection */
    template <RT(*fun_ptr)(Args...)>
    auto connect() -> observer::Connection&
    {
        return observer::insert(function::template bind<fun_ptr>(), this);
    }
    /* connect to a member method of a class T passed by pointer*/
    template <typename T, RT(T::*mem_ptr)(Args...)>
    auto connect(T* instance) -> observer::Connection&
    {
        return insert_sfinae<T>(function::template bind<mem_ptr>(instance), instance);
    }
    /* connect to a member const method of a class T passed by pointer */
    template <typename T, RT(T::*mem_ptr)(Args...) const>
    auto connect(T* instance) -> observer::Connection&
    {
        return insert_sfinae<T>(function::template bind<mem_ptr>(instance), instance);
    }
    /* connect to a member method of a class T passed by ref*/
    template <typename T, RT(T::*mem_ptr)(Args...)>
    auto connect(T& instance) -> observer::Connection&
    {
        return connect<mem_ptr, T>(std::addressof(instance));
    }
    /* connect to a member const method of a class T passed by ref */
    template <typename T, RT(T::*mem_ptr)(Args...) const>
    auto connect(T& instance) -> observer::Connection&
    {
        return connect<mem_ptr, T>(std::addressof(instance));
    }
    /* implementions detail of the connection to a member method of a class T passed by pointer*/
    template <auto mem_ptr, typename T>
    auto connect(T* instance) -> observer::Connection&
    {
        return insert_sfinae<T>(function::template bind<mem_ptr>(instance), instance);
    }
    /* implementions detail of the connection to a member method of a class T passed by ref*/
    template <auto mem_ptr, typename T>
    auto connect(T& instance) -> observer::Connection&
    {
        return connect<mem_ptr, T>(std::addressof(instance));
    }
    /* implementions detail of the connection to a functor ptr */
    template <auto mem_ptr>
    auto connect() -> observer::Connection&
    {
        return observer::insert(function::template bind<mem_ptr>(), this);
    }

    //----------------------------------------------------------------DISCONNECT

    template <typename L>
    void disconnect(L* instance)
    {
        observer::remove(function::template bind<decltype(instance)>(instance));
    }
    template <typename L>
    void disconnect(L& instance)
    {
        disconnect(std::addressof(instance));
    }

    template <RT(*fun_ptr)(Args...)>
    void disconnect()
    {
        observer::remove(function::template bind<fun_ptr>());
    }

    template <typename T, RT(T::*mem_ptr)(Args...)>
    void disconnect(T* instance)
    {
        remove_sfinae<T>(function::template bind<mem_ptr>(instance), instance);
    }
    template <typename T, RT(T::*mem_ptr)(Args...) const>
    void disconnect(T* instance)
    {
        remove_sfinae<T>(function::template bind<mem_ptr>(instance), instance);
    }

    template <typename T, RT(T::*mem_ptr)(Args...)>
    void disconnect(T& instance)
    {
        disconnect<T, mem_ptr>(std::addressof(instance));
    }
    template <typename T, RT(T::*mem_ptr)(Args...) const>
    void disconnect(T& instance)
    {
        disconnect<T, mem_ptr>(std::addressof(instance));
    }

    template <auto mem_ptr, typename T>
    void disconnect(T* instance)
    {
        remove_sfinae<T>(function::template bind<mem_ptr>(instance), instance);
    }
    template <auto mem_ptr, typename T>
    void disconnect(T& instance)
    {
        disconnect<mem_ptr, T>(std::addressof(instance));
    }

    //----------------------------------------------------FIRE / FIRE ACCUMULATE

    template <typename... Uref>
    requires(!is_awaitable_type_v<RT>)
    void emit(Uref&&... args)
    {
        observer::template for_each<function>(std::forward<Uref>(args)...);
    }

    template <typename... Uref>
    requires(!is_awaitable_type_v<RT>)
    void operator()(Uref&&... args)
    {
        observer::template for_each<function>(std::forward<Uref>(args)...);
    }

    template <typename Accumulate, typename... Uref>
    void fire_accumulate(Accumulate&& accumulate, Uref&&... args)
    {
        observer::template for_each_accumulate<function, Accumulate>
            (std::forward<Accumulate>(accumulate), std::forward<Uref>(args)...);
    }

#if __has_include("boost/asio.hpp") || __has_include("asio.hpp")
    template <typename... Uref>
    requires(is_awaitable_type_v<RT>)
    //std::enable_if_t<is_asio_awaitable_v<RT>, boost::asio::awaitable<void>>
    asio::awaitable<void>
    emit(Uref&&... args)
    {
        co_await observer::template coro_for_each<function>(std::forward<Uref>(args)...);
    }
    template <typename... Uref>
        requires(is_awaitable_type_v<RT>)
    //std::enable_if_t<is_asio_awaitable_v<RT>, boost::asio::awaitable<void>>
    asio::awaitable<void>
    emit(Uref&&... args) const
    {
        co_await observer::template coro_for_each<function>(std::forward<Uref>(args)...);
    }

    template <typename... Uref>
    //std::enable_if_t<is_asio_awaitable_v<RT>, boost::asio::awaitable<void>>
    asio::awaitable<void>
    operator()(Uref&&... args)
    {
        co_await observer::template coro_for_each<function>(std::forward<Uref>(args)...);
    }
//    template <typename Accumulate, typename... Uref>
//    asio::awaitable<void> fire_accumulate(Accumulate&& accumulate, Uref&&... args)
//    {
//        observer::template for_each_accumulate<function, Accumulate>
//            (std::forward<Accumulate>(accumulate), std::forward<Uref>(args)...);
//    }
#endif


};

} // namespace Nano ------------------------------------------------------------
