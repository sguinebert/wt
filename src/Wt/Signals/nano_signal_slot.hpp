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
    void insert_sfinae(Delegate_Key const& key, typename T::Observer* instance)
    {
        auto conn = observer::insert(key, instance);
        instance->insert(key, this);
        //return conn;
    }
    template <typename T>
    void remove_sfinae(Delegate_Key const& key, typename T::Observer* instance)
    {
        observer::remove(key);
        instance->remove(key);
    }
    template <typename T>
    void insert_sfinae(Delegate_Key const& key, ...)
    {
        observer::insert(key, this);
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
        return Observer<MT_Policy>::Connection(key, this);
    }
    template <typename L>
    Observer<MT_Policy>::Connection make_Connection(L* function)
    {
        Delegate_Key key = function::template bind(function);
        return Observer<MT_Policy>::Connection(key, this);
    }

    //-------------------------------------------------------------------CONNECT

    template <typename L>
    void connect(L* instance)
    {
        observer::insert(function::template bind(instance), this);
    }
    /* connect to a lambda or std::bind callable object passed by r or l-value */
    template <typename L>
    observer::Connection connect(L&& instance)
    {

        /* it is a reference to a functor (example a lambda passed by ref) - you need to watch the lifetime of the lambda */
        if constexpr(std::is_lvalue_reference_v<L>) {
            connect(std::addressof(instance));
            return typename observer::Connection(function::template bind(std::addressof(instance)), this);
        }
        /* the size of the object L is less or equal than the size of a pointer : rational -> if the size is a pointer then no internal state present in the lambda so no save needed on the heap */
//        else if constexpr (sizeof(std::remove_pointer_t<L>) <= sizeof(void*))
//        {
//            return connect(std::addressof(instance));
//        }
        /* allocate & copy the lambda on the heap and keep shared_ptr alive in a std::any object inside a Connection object*/
        else {
            using f_type = std::remove_pointer_t<std::remove_reference_t<L>>;

            //auto shrd = std::shared_ptr<f_type>(new f_type(std::move(instance)));
            auto shrd = std::make_shared<f_type>(std::move(instance));
            //std::cerr << "test rvalue lambda store and call " << std::addressof(*shrd) << " // " << shrd.get() << std::endl;
            observer::insert(function::template bind(shrd.get()), this, std::move(shrd));
            return typename observer::Connection(function::template bind(shrd.get()), this);
        }
    }
    template <typename L, typename T>
    observer::Connection connect(L&& func, T* instance)
    {

        /* it is a reference to a functor (example a lambda passed by ref) - you need to watch the lifetime of the lambda */
        if constexpr(std::is_lvalue_reference_v<L>) {
            insert_sfinae<T>(function::template bind<std::addressof(func)>(instance), instance);
            return typename observer::Connection(function::template bind(std::addressof(func)), this);
            //return connect(std::addressof(func), instance);
        }
        /* the size of the object L is less than the size of a pointer */
        //        else if constexpr (sizeof(std::remove_pointer_t<L>) <= sizeof(void*))
        //        {
        //            return insert_sfinae<T>(function::template bind<std::addressof(func)>(instance), instance);
        //        }
        /* allocate & copy the lambda in the heap and keep shared_ptr in std::any inside Connection*/
        else {
            using f_type = std::remove_pointer_t<std::remove_reference_t<L>>;

            //instance->insert(function::template bind<&func>(instance), this);
            //auto shrd = std::shared_ptr<f_type>(new f_type(std::move(instance)));
            auto shrd = std::make_shared<f_type>(std::move(instance));
            //std::cerr << "test rvalue lambda store and call " << std::addressof(*shrd) << " // " << shrd.get() << std::endl;
            insert_sfinae<T>(function::template bind<shrd.get()>(instance), instance);
            //return observer::insert(function::template bind(shrd.get()), this, std::move(shrd));
            return typename observer::Connection(function::template bind(shrd.get()), this);
        }
    }

    /* static function connection */
    template <RT(*fun_ptr)(Args...)>
    void connect()
    {
        observer::insert(function::template bind<fun_ptr>(), this);
    }
    /* connect to a member method of a class T passed by pointer*/
    template <typename T, RT(T::*mem_ptr)(Args...)>
    void connect(T* instance)
    {
        insert_sfinae<T>(function::template bind<mem_ptr>(instance), instance);
    }
    /* connect to a member const method of a class T passed by pointer */
    template <typename T, RT(T::*mem_ptr)(Args...) const>
    void connect(T* instance)
    {
        insert_sfinae<T>(function::template bind<mem_ptr>(instance), instance);
    }
    /* connect to a member method of a class T passed by ref*/
    template <typename T, RT(T::*mem_ptr)(Args...)>
    void connect(T& instance)
    {
        connect<mem_ptr, T>(std::addressof(instance));
    }
    /* connect to a member const method of a class T passed by ref */
    template <typename T, RT(T::*mem_ptr)(Args...) const>
    void connect(T& instance)
    {
        connect<mem_ptr, T>(std::addressof(instance));
    }
    /* implementions detail of the connection to a member method of a class T passed by pointer*/
    template <auto mem_ptr, typename T>
    void connect(T* instance)
    {
        insert_sfinae<T>(function::template bind<mem_ptr>(instance), instance);
    }
    /* implementions detail of the connection to a member method of a class T passed by ref*/
    template <auto mem_ptr, typename T>
    void connect(T& instance)
    {
        connect<mem_ptr, T>(std::addressof(instance));
    }
    /* implementions detail of the connection to a functor ptr */
    template <auto mem_ptr>
    void connect()
    {
        observer::insert(function::template bind<mem_ptr>(), this);
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
