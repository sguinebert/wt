#pragma once

#include <algorithm>
#include <vector>

#include "nano_function.hpp"
#include "nano_mutex.hpp"
#include <iostream>

#if __has_include("boost/asio.hpp")
#include <boost/asio.hpp>
using namespace boost;
#elif defined(ASIO_HPP)
#include <asio.hpp>
#endif

namespace Nano
{

template <typename MT_Policy = ST_Policy>
class Observer : private MT_Policy
{
    // Only Nano::Signal is allowed private access
    template <typename, typename> friend class Signal;
public:
    struct Connection
    {
        Delegate_Key delegate;
        typename MT_Policy::Weak_Ptr observer;
        std::any any; //o

        Connection() noexcept = default;
        Connection(Delegate_Key const& key) noexcept : delegate(key), observer() {}
        Connection(Delegate_Key const& key, Observer* obs) noexcept : delegate(key), observer(obs->weak_ptr()) {}
        template<class T>
        Connection(Delegate_Key const& key, Observer* obs, T&& ptr) noexcept : delegate(key), observer(obs->weak_ptr()), any(std::forward<T>(ptr))
        {
//            std::cout << "Connection addressof : " << std::addressof(*ptr) << std::endl;
//            auto lambda = std::any_cast<T>(any);
//            std::cout << "Connection any addressof : " << std::addressof(*lambda) << std::endl;
//            std::cout << "test : " << reinterpret_cast<void*>(delegate[0]) << " - " << reinterpret_cast<void*>(delegate[1]) << std::endl;
        }
        Connection(const Connection& other) : delegate(other.delegate), observer(other.observer)
        {
            std::cout << "Connection copy" << std::endl;
        }
        Connection(const Connection&& other) : delegate(other.delegate), observer(other.observer)
        {
            std::cout << "Connection move" << std::endl;
        }
        Connection& operator=(const Connection& other) noexcept {
            std::cout << "copy assignment " << std::endl;
            delegate = other.delegate;
            //observer = other.observer->weak_ptr();
            return *this;
        }
        template<class O>
        Connection& operator=(const O& other) noexcept {
            std::cout << "copy assignment " << std::endl;
            delegate = other.delegate;
            //observer = other.observer->weak_ptr();
            return *this;
        }

        void disconnect() noexcept {
            if (auto observed = observer->visiting(observer))
            {
                auto ptr = static_cast<Observer*>(observer->unmask(observer));
                ptr->remove(delegate);
            }
        }
        bool isConnected() const noexcept {
            if (auto observed = observer->visiting(observer))
            {
                auto ptr = static_cast<Observer*>(observer->unmask(observer));
                return ptr->nolock_isConnected(delegate);
            }
            return false;
        }
    };
private:
    struct Z_Order
    {
        inline bool operator()(Delegate_Key const& lhs, Delegate_Key const& rhs) const
        {
            std::size_t x = lhs[0] ^ rhs[0];
            std::size_t y = lhs[1] ^ rhs[1];
            auto k = (x < y) && x < (x ^ y);
            return lhs[k] < rhs[k];
        }

        inline bool operator()(Connection const& lhs, Connection const& rhs) const
        {
            return operator()(lhs.delegate, rhs.delegate);
        }

        inline bool operator()(std::unique_ptr<Connection> const& lhs, std::unique_ptr<Connection> const& rhs) const
        {
            return operator()(lhs->delegate, rhs->delegate);
        }

        inline bool operator()(std::unique_ptr<Connection> const& lhs, Delegate_Key const& rhs) const
        {
            return operator()(lhs->delegate, rhs);
        }

        inline bool operator()(Delegate_Key const& lhs, std::unique_ptr<Connection> const& rhs) const
        {
            return operator()(lhs, rhs->delegate);
        }
    };

    std::vector<Connection> connections;

    //--------------------------------------------------------------------------
    bool nolock_isConnected(Delegate_Key const& key) const noexcept
    {
//        auto begin = std::begin(connections);
//        auto end = std::end(connections);

        for (auto& conn : connections) {
            if(conn.delegate[0] == key[0] && conn.delegate[1] == key[1])
                return true;
        }
        return false;
    }
    bool isConnected(Delegate_Key const& key)
    {
        [[maybe_unused]]
        auto lock = MT_Policy::lock_guard();

        return nolock_isConnected(key);
    }

    Connection nolock_insert(Delegate_Key const& key, Observer* obs)
    {
        auto begin = std::begin(connections);
        auto end = std::end(connections);

        return *connections.emplace(std::upper_bound(begin, end, key, Z_Order()), key, obs);
    }

    Connection insert(Delegate_Key const& key, Observer* obs)
    {
        [[maybe_unused]]
        auto lock = MT_Policy::lock_guard();

        return nolock_insert(key, obs);
    }
    template<class T>
    Connection insert(Delegate_Key const& key, Observer* obs, T&& ptr)
    {
        [[maybe_unused]]
        auto lock = MT_Policy::lock_guard();

        auto begin = std::begin(connections);
        auto end = std::end(connections);

        return *connections.emplace(std::upper_bound(begin, end, key, Z_Order()), key, obs, std::forward<T>(ptr));
    }

    void remove(Delegate_Key const& key) noexcept
    {
        [[maybe_unused]]
        auto lock = MT_Policy::lock_guard();

        auto begin = std::begin(connections);
        auto end = std::end(connections);

        auto slots = std::equal_range(begin, end, key, Z_Order());
        connections.erase(slots.first, slots.second);
    }

    //--------------------------------------------------------------------------

    template <typename Function, typename... Uref>
    void for_each(Uref&&... args)
    {
        [[maybe_unused]]
        auto lock = MT_Policy::lock_guard();

        for (auto const& slot : MT_Policy::copy_or_ref(connections, lock))
        {
            if (auto observer = MT_Policy::observed(slot.observer))
            {
                Function::bind(slot.delegate)(args...);
            }
        }
    }

    template <typename Function, typename... Uref>
    void for_each(Uref&&... args) const
    {
        [[maybe_unused]]
        auto lock = MT_Policy::lock_guard();

        for (auto const& slot : MT_Policy::copy_or_ref(connections, lock))
        {
            if (auto observer = MT_Policy::observed(slot.observer))
            {
                Function::bind(slot.delegate)(args...);
            }
        }
    }
    template <typename Function, typename Accumulate, typename... Uref>
    void for_each_accumulate(Accumulate&& accumulate, Uref&&... args)
    {
        [[maybe_unused]]
        auto lock = MT_Policy::lock_guard();

        for (auto const& slot : MT_Policy::copy_or_ref(connections, lock))
        {
            if (auto observer = MT_Policy::observed(slot.observer))
            {
                accumulate(Function::bind(slot.delegate)(args...));
            }
        }
    }

#if __has_include("boost/asio.hpp") || __has_include("asio.hpp")
    template <typename Function, typename... Uref>
    asio::awaitable<void> coro_for_each(Uref&&... args)
    {
        [[maybe_unused]]
        auto lock = MT_Policy::lock_guard();

        for (auto const& slot : MT_Policy::copy_or_ref(connections, lock))
        {
            if (auto observer = MT_Policy::observed(slot.observer))
            {
                co_await Function::bind(slot.delegate)(args...);
            }
        }
        co_return;
    }

    template <typename Function, typename... Uref>
    asio::awaitable<void> coro_for_each(Uref&&... args) const
    {
        [[maybe_unused]]
        auto lock = MT_Policy::lock_guard();

        for (auto const& slot : MT_Policy::copy_or_ref(connections, lock))
        {
            if (auto observer = MT_Policy::observed(slot.observer))
            {
                co_await Function::bind(slot.delegate)(args...);
            }
        }
        co_return;
    }

    template <typename Function, typename Accumulate, typename... Uref>
    asio::awaitable<void> for_each_accumulate(Accumulate&& accumulate, Uref&&... args)
    {
        [[maybe_unused]]
        auto lock = MT_Policy::lock_guard();

        for (auto const& slot : MT_Policy::copy_or_ref(connections, lock))
        {
            if (auto observer = MT_Policy::observed(slot.observer))
            {
                accumulate(co_await Function::bind(slot.delegate)(args...));
            }
        }
    }
#endif



    //--------------------------------------------------------------------------

    void nolock_disconnect_all() noexcept
    {
        for (auto const& slot : connections)
        {
            if (auto observed = MT_Policy::visiting(slot.observer))
            {
                auto ptr = static_cast<Observer*>(MT_Policy::unmask(observed));
                ptr->remove(slot.delegate);
            }
        }

        connections.clear();
    }

    void move_connections_from(Observer* other) noexcept
    {
        [[maybe_unused]]
        auto lock = MT_Policy::scoped_lock(other);

        // Make sure this is disconnected and ready to receive
        nolock_disconnect_all();

        // Disconnect other from everyone else and connect them to this
        for (auto const& slot : other->connections)
        {
            if (auto observed = other->visiting(slot.observer))
            {
                auto ptr = static_cast<Observer*>(MT_Policy::unmask(observed));
                ptr->remove(slot.delegate);
                ptr->insert(slot.delegate, this);
                nolock_insert(slot.delegate, ptr);
            }
            // Connect free functions and function objects
            else
            {
                nolock_insert(slot.delegate, this);
            }
        }

        other->connections.clear();
    }

    //--------------------------------------------------------------------------

    public:

    void disconnect_all() noexcept
    {
        [[maybe_unused]]
        auto lock = MT_Policy::lock_guard();

        nolock_disconnect_all();
    }

    bool is_empty() const noexcept
    {
        [[maybe_unused]]
        auto lock = MT_Policy::lock_guard();

        return connections.empty();
    }

    bool isConnected() const noexcept
    {
        [[maybe_unused]]
        auto lock = MT_Policy::lock_guard();

        return connections.empty();
    }

    protected:

    // Guideline #4: A base class destructor should be
    // either public and virtual, or protected and non-virtual.
    ~Observer()
    {
        MT_Policy::before_disconnect_all();

        disconnect_all();
    }

    Observer() noexcept = default;

    // Observer may be movable depending on policy, but should never be copied
    Observer(Observer const&) noexcept = delete;
    Observer& operator= (Observer const&) noexcept = delete;

    // When moving an observer, make sure everyone it's connected to knows about it
    Observer(Observer&& other) noexcept
    {
        move_connections_from(std::addressof(other));
    }

    Observer& operator=(Observer&& other) noexcept
    {
        move_connections_from(std::addressof(other));
        return *this;
    }
};

} // namespace Nano ------------------------------------------------------------
