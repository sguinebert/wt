#pragma once

#include "nano_function.hpp"
#include "nano_observer.hpp"
#include <iostream>

namespace Nano
{

//namespace Signals {
//    using Connection = Observer<MT_Policy>::Connection;
//}

template <typename R>
struct is_asio_awaitable : std::false_type {};

template <typename R>
struct is_asio_awaitable<boost::asio::awaitable<R>> : std::true_type {};

template <typename R>
inline constexpr bool is_asio_awaitable_v = is_asio_awaitable<R>::value;

template <typename RT, typename MT_Policy = ST_Policy>
class Signal;
template <typename RT, typename MT_Policy, typename... Args>
class Signal<RT(Args...), MT_Policy> final : public Observer<MT_Policy>
{
    using observer = Observer<MT_Policy>;
    using function = Function<RT(Args...)>;

    template <typename T>
    observer::Connection insert_sfinae(Delegate_Key const& key, typename T::Observer* instance)
    {
        auto conn = observer::insert(key, instance);
        instance->insert(key, this);
        return conn;
    }
    template <typename T>
    void remove_sfinae(Delegate_Key const& key, typename T::Observer* instance)
    {
        observer::remove(key);
        instance->remove(key);
    }
    template <typename T>
    observer::Connection insert_sfinae(Delegate_Key const& key, ...)
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

    //-------------------------------------------------------------------CONNECT

    template <typename L>
    observer::Connection connect(L* instance)
    {
        return observer::insert(function::template bind(instance), this);
    }
    template <typename L>
    observer::Connection connect(L&& instance)
    {

        /* it is a reference to a functor (example a lambda passed by ref) */
        if constexpr(std::is_lvalue_reference_v<L>) {
            return connect(std::addressof(instance));
        }
        /* the size of the object L is less than the size of a pointer */
//        else if constexpr (sizeof(std::remove_pointer_t<L>) <= sizeof(void*))
//        {
//        }
        /* allocate & copy the lambda in the heap and keep shared_ptr in std::any inside Connection*/
        else {
            using f_type = std::remove_pointer_t<std::remove_reference_t<L>>;

            //auto shrd = std::shared_ptr<f_type>(new f_type(std::move(instance)));
            auto shrd = std::make_shared<f_type>(std::move(instance));
            //std::cerr << "test rvalue lambda store and call " << std::addressof(*shrd) << " // " << shrd.get() << std::endl;
            return observer::insert(function::template bind(shrd.get()), this, std::move(shrd));
        }
    }
    /* static function connection */
    template <RT(*fun_ptr)(Args...)>
    observer::Connection connect()
    {
        return observer::insert(function::template bind<fun_ptr>(), this);
    }
    /* connect to a member method of a class T passed by pointer*/
    template <typename T, RT(T::*mem_ptr)(Args...)>
    observer::Connection connect(T* instance)
    {
        return insert_sfinae<T>(function::template bind<mem_ptr>(instance), instance);
    }
    /* connect to a member const method of a class T passed by pointer */
    template <typename T, RT(T::*mem_ptr)(Args...) const>
    observer::Connection connect(T* instance)
    {
        return insert_sfinae<T>(function::template bind<mem_ptr>(instance), instance);
    }
    /* connect to a member method of a class T passed by ref*/
    template <typename T, RT(T::*mem_ptr)(Args...)>
    observer::Connection connect(T& instance)
    {
        return connect<mem_ptr, T>(std::addressof(instance));
    }
    /* connect to a member const method of a class T passed by ref */
    template <typename T, RT(T::*mem_ptr)(Args...) const>
    observer::Connection connect(T& instance)
    {
        return connect<mem_ptr, T>(std::addressof(instance));
    }
    /* implementions detail of the connection to a member method of a class T */
    template <auto mem_ptr, typename T>
    observer::Connection connect(T* instance)
    {
        return insert_sfinae<T>(function::template bind<mem_ptr>(instance), instance);
    }
    template <auto mem_ptr, typename T>
    observer::Connection connect(T& instance)
    {
        return connect<mem_ptr, T>(std::addressof(instance));
    }
    template <auto mem_ptr>
    observer::Connection connect()
    {
        return observer::insert(function::template bind<mem_ptr>(), this);
    }

    //----------------------------------------------------------------DISCONNECT

    template <typename L>
    void disconnect(L* instance)
    {
        observer::remove(function::template bind(instance));
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
    requires(!is_asio_awaitable_v<RT>)
    void emit(Uref&&... args)
    {
        observer::template for_each<function>(std::forward<Uref>(args)...);
    }

    template <typename... Uref>
    requires(is_asio_awaitable_v<RT>)
    //std::enable_if_t<is_asio_awaitable_v<RT>, boost::asio::awaitable<void>>
    boost::asio::awaitable<void>
    emit(Uref&&... args)
    {
        co_await observer::template coro_for_each<function>(std::forward<Uref>(args)...);
    }

    template <typename... Uref>
        requires(is_asio_awaitable_v<RT>)
    //std::enable_if_t<is_asio_awaitable_v<RT>, boost::asio::awaitable<void>>
    boost::asio::awaitable<void>
    emit(Uref&&... args) const
    {
        co_await observer::template coro_for_each<function>(std::forward<Uref>(args)...);
    }

    template <typename... Uref>
        requires(!is_asio_awaitable_v<RT>)
    void operator()(Uref&&... args)
    {
        observer::template for_each<function>(std::forward<Uref>(args)...);
    }

    template <typename... Uref>
    //std::enable_if_t<is_asio_awaitable_v<RT>, boost::asio::awaitable<void>>
    boost::asio::awaitable<void>
    operator()(Uref&&... args)
    {
        co_await observer::template coro_for_each<function>(std::forward<Uref>(args)...);
    }

    template <typename Accumulate, typename... Uref>
    void fire_accumulate(Accumulate&& accumulate, Uref&&... args)
    {
        observer::template for_each_accumulate<function, Accumulate>
            (std::forward<Accumulate>(accumulate), std::forward<Uref>(args)...);
    }
};

} // namespace Nano ------------------------------------------------------------
