#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <future>

namespace Nano
{

namespace internal
{
// Helper to detect operator co_await
template <typename T, typename = void>
struct has_operator_co_await : std::false_type {};

template <typename T>
struct has_operator_co_await<T, std::void_t<decltype(std::declval<T>().operator co_await())>> : std::true_type {};

// Helper to detect await_resume
template <typename T, typename = void>
struct has_await_resume : std::false_type {};

template <typename T>
struct has_await_resume<T, std::void_t<decltype(std::declval<T>().await_resume())>> : std::true_type {};

}  // end namespace internal

//default
template<typename T, typename = void>
struct contains_awaitable : std::false_type {};

// Specialization for types with operator co_await or for types with await_resume
template <typename T>
struct contains_awaitable<T, std::enable_if_t<internal::has_operator_co_await<T>::value ||
                                              internal::has_await_resume<T>::value>> : std::true_type {};

// Check for std::future specializations
template<typename T>
struct contains_awaitable<std::future<T>> : std::true_type {};

// Check for boost::asio::awaitable specializations
//template<typename R>
//struct contains_awaitable<boost::asio::awaitable<R>> : std::true_type {};

template <typename R>
inline constexpr bool is_awaitable_type_v = contains_awaitable<R>::value;

template<typename Func>
struct contains_awaitable_impl {
    static constexpr bool value = false;
};

//template<typename R, typename... Args>
//struct contains_awaitable_impl<boost::asio::awaitable<R>(Args...)> : std::true_type {};

template<typename R, typename... Args>
struct contains_awaitable_impl<R(Args...)> : contains_awaitable<R> {};

template <typename R, typename... _Args>
inline constexpr bool is_awaitable_v = contains_awaitable_impl<R(_Args...)>::value;

template <typename F>
struct function_traits : public function_traits<decltype(&std::decay_t<F>::operator())>
{
//    using return_type = std::invoke_result_t<F>;
//    inline static bool constexpr value = contains_awaitable<return_type>::value;
};

// Specialization for std::bind
#if defined _LIBCPP_VERSION  // libc++ (Clang)
template <typename R, typename... Args>
struct function_traits<std::__bind<R (Args...)>> {
    using function_type = std::function<std::remove_pointer_t<R>>;
    inline static bool constexpr value = contains_awaitable_impl<std::remove_pointer_t<R>>::value;
};
#elif defined _GLIBCXX_RELEASE  // glibc++ (GNU C++ >= 7.1)
template <typename R, typename... Args>
struct function_traits<std::_Bind<R (Args...)>> {
    using function_type = std::function<std::remove_pointer_t<R>>;
    inline static bool constexpr value = contains_awaitable_impl<std::remove_pointer_t<R>>::value; //std::is_convertible_v<function_type, ccc>;
};
// std::bind for object methods
template<typename C, typename R, typename ... FArgs, typename ... Args>
struct function_traits<std::_Bind<R(C::*(FArgs ...))(Args ...)>>
{
    using function_type = std::function<R(FArgs ...)>;
    inline static bool constexpr value = contains_awaitable_impl<R(FArgs ...)>::value;
};
#elif defined _MSC_VER  // MS Visual Studio
template <typename R, typename... Args>
struct function_traits<std::_Binder<std::_Unforced, R (Args...)>> {
    using function_type = std::function<std::remove_pointer_t<R>>;
    inline static bool constexpr value = contains_awaitable_impl<std::remove_pointer_t<R>>::value;
};
#else
#error "Unsupported C++ compiler / standard library"
#endif


template <typename R, typename C, typename... Args >
struct function_traits<R (C::*)(Args...)>
{
    using function_type = std::function<R (Args...)>;
    inline static bool constexpr value = contains_awaitable_impl<R (Args...)>::value;
};

template <typename R, typename C, typename... Args >
struct function_traits<R (C::*)(Args...) const>
{
    using function_type = std::function<R (Args...)>;
    inline static bool constexpr value = contains_awaitable_impl<R (Args...)>::value;
};

template <typename F>
constexpr bool is_awaitable_callable_v = function_traits<F>::value;

template <typename F>
using is_awaitable_lambda_t = function_traits<F>::function_traits::function_type;

using Delegate_Key = std::array<std::uintptr_t, 2>;

template <typename RT> class Function;
template <typename RT, typename... Args>
class Function<RT(Args...)> final
{
    // Only Nano::Observer is allowed private access
    template <typename> friend class Observer;

    using Thunk = RT(*)(void*, Args&&...);

    static inline Function bind(Delegate_Key const& delegate_key)
    {
        return
        {
            reinterpret_cast<void*>(delegate_key[0]),
            reinterpret_cast<Thunk>(delegate_key[1])
        };
    }

    public:

    void* const instance_pointer;
    const Thunk function_pointer;

    template <auto fun_ptr>
    static inline Function bind()
    {
        using f_type = std::remove_pointer_t<std::remove_reference_t<decltype(fun_ptr)>>;
        if constexpr(is_awaitable_type_v<RT> && !is_awaitable_callable_v<f_type>)
        {
            return
                {
                    nullptr, [](void* /*NULL*/, Args&&... args) -> RT
                    {
                    if constexpr(std::is_invocable_v<f_type>) //just want a callback without any args
                        co_return (*fun_ptr)();
                    else
                        co_return (*fun_ptr)(std::forward<Args>(args)...);
                    }
                };
        }
        else {
            return
            {
                nullptr, [](void* /*NULL*/, Args&&... args)
                {
                    if constexpr(std::is_invocable_v<f_type>)
                        return (*fun_ptr)();
                    else
                        return (*fun_ptr)(std::forward<Args>(args)...);
                }
            };
        }
    }

    template <auto mem_ptr, typename T>
    static inline Function bind(T* pointer)
    {
        using f_type = std::remove_pointer_t<std::remove_reference_t<decltype(mem_ptr)>>;
        if constexpr(is_awaitable_type_v<RT> && !is_awaitable_callable_v<f_type>)
        {
            return
                {
                    pointer, [](void* this_ptr, Args&&... args) -> RT
                    {
                        if constexpr(std::is_invocable_v<f_type, T*>)
                            co_return (static_cast<T*>(this_ptr)->*mem_ptr)();
                        else
                            co_return (static_cast<T*>(this_ptr)->*mem_ptr)(std::forward<Args>(args)...);
                    }
                };
        }
        else {
            return
                {
                    pointer, [](void* this_ptr, Args&&... args)
                    {
                        if constexpr(std::is_invocable_v<f_type, T*>)
                            return (static_cast<T*>(this_ptr)->*mem_ptr)();
                        else
                            return (static_cast<T*>(this_ptr)->*mem_ptr)(std::forward<Args>(args)...);
                    }
                };
        }
    }


    template <typename L>
    static inline Function bind(L* pointer)
    {//std::is_convertible_v<RT, boost::asio::awaitable<void>>
        using ReturnType = std::invoke_result_t<L, Args...>;
        if constexpr(is_awaitable_type_v<RT> && (/*!is_awaitable_lambda_v<L> &&*/ !is_awaitable_type_v<ReturnType>)) {
            return
                {
                    pointer, [](void* this_ptr, Args&&... args) -> RT
                    {
                        co_return static_cast<L*>(this_ptr)->operator()(std::forward<Args>(args)...);
                    }
                };
        }
        else {
            return
                {
                    pointer, [](void* this_ptr, Args&&... args)
                    {
                        return static_cast<L*>(this_ptr)->operator()(std::forward<Args>(args)...);
                    }
                };
        }
    }

    template <typename... Uref>
    inline RT operator() (Uref&&... args) const
    {
        return (*function_pointer)(instance_pointer, static_cast<Args&&>(args)...);
    }

    inline operator Delegate_Key() const
    {
        return
        {
            reinterpret_cast<std::uintptr_t>(instance_pointer),
            reinterpret_cast<std::uintptr_t>(function_pointer)
        };
    }
};

} // namespace Nano ------------------------------------------------------------
