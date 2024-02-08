#ifndef ASYNC_MUTEX_H
#define ASYNC_MUTEX_H

#include <boost/asio.hpp>
#include <Wt/cuehttp/detail/MoveOnlyFunction.hpp>

#include <atomic>
#include <cstdint>
#include <queue>
#include <mutex> // for std::adopt_lock_t

using namespace boost;

namespace Wt {
namespace cpp20 {

    class async_mutex_lock;
    class async_mutex;
    class async_mutex_scoped_lock_operation;



    class async_mutex_noscope
    {
    };

    class async_mutex_lock_operation
    {
    public:

        explicit async_mutex_lock_operation(async_mutex& mutex) noexcept
            : m_mutex(mutex)
        {}

    protected:

        friend class async_mutex;

        async_mutex& m_mutex;

    private:

        async_mutex_lock_operation* m_next;
        Wt::cpp23::move_only_function<void(async_mutex_noscope)> m_awaiter; //store the asio handler
        asio::any_io_executor* m_context;
        bool m_scopelocked = false;

    };


    /// \brief
    /// An object that holds onto a mutex lock for its lifetime and
    /// ensures that the mutex is unlocked when it is destructed.
    ///
    /// It is equivalent to a std::lock_guard object but requires
    /// that the result of co_await async_mutex::lock_async() is
    /// passed to the constructor rather than passing the async_mutex
    /// object itself.
    class async_mutex_lock: public async_mutex_noscope
    {
    public:

        explicit async_mutex_lock(async_mutex& mutex, std::adopt_lock_t) noexcept
            : m_mutex(&mutex)
        {}

        async_mutex_lock(async_mutex_lock&& other) noexcept
            : m_mutex(other.m_mutex)
        {
            other.m_mutex = nullptr;
        }

        async_mutex_lock(const async_mutex_lock& other) = delete;
        async_mutex_lock& operator=(const async_mutex_lock& other) = delete;

        // Releases the lock.
        ~async_mutex_lock();

    private:

        async_mutex* m_mutex;

    };

    /// \brief
    /// A mutex that can be locked asynchronously using 'co_await'.
    ///
    /// Ownership of the mutex is not tied to any particular thread.
    /// This allows the coroutine owning the lock to transition from
    /// one thread to another while holding a lock.
    ///
    /// Implementation is lock-free, using only std::atomic values for
    /// synchronisation. Awaiting coroutines are suspended without blocking
    /// the current thread if the lock could not be acquired synchronously.
    class async_mutex
    {
    public:

        /// \brief
        /// Construct to a mutex that is not currently locked.
        async_mutex() noexcept;

        /// Destroys the mutex.
        ///
        /// Behaviour is undefined if there are any outstanding coroutines
        /// still waiting to acquire the lock.
        ~async_mutex();

        /// \brief
        /// Attempt to acquire a lock on the mutex without blocking.
        ///
        /// \return
        /// true if the lock was acquired, false if the mutex was already locked.
        /// The caller is responsible for ensuring unlock() is called on the mutex
        /// to release the lock if the lock was acquired by this call.
        bool try_lock() noexcept;

        /// \brief
        /// Acquire a lock on the mutex asynchronously.
        ///
        /// If the lock could not be acquired synchronously then the awaiting
        /// coroutine will be suspended and later resumed when the lock becomes
        /// available. If suspended, the coroutine will be resumed inside the
        /// call to unlock() from the previous lock owner.
        ///
        /// \return
        /// An operation object that must be 'co_await'ed to wait until the
        /// lock is acquired. The result of the 'co_await m.lock_async()'
        /// expression has type 'void'.
        template<class Token>
        auto lock_async(Token&& handler, bool scoped = false) {
            //async_mutex_lock_operation ad(*this);
            auto lock = std::make_shared<async_mutex_lock_operation>(*this); //allocate on the stack FIX deleted constructor)
            lock->m_scopelocked = scoped;

            auto initiation = [this, lock](auto&& handler) {

                auto executor = asio::get_associated_executor(handler);
                std::uintptr_t oldState = m_state.load(std::memory_order_acquire);
                while (true)
                {
                    if (oldState == async_mutex::not_locked)
                    {
                        if (m_state.compare_exchange_weak(
                                oldState,
                                async_mutex::locked_no_waiters,
                                std::memory_order_acquire,
                                std::memory_order_relaxed))
                        {
                            // Acquired lock, don't suspend.
                            handler(lock->m_scopelocked ? async_mutex_lock {*this, std::adopt_lock} : async_mutex_noscope());
                            return;
                        }
                    }
                    else
                    {
                        // Try to push this operation onto the head of the waiter stack.
                        lock->m_next = reinterpret_cast<async_mutex_lock_operation*>(oldState);
                        lock->m_context = &executor;
                        if (m_state.compare_exchange_weak(
                                oldState,
                                reinterpret_cast<std::uintptr_t>(lock.get()),
                                std::memory_order_release,
                                std::memory_order_relaxed))
                        {
                            // Queued operation to waiters list, suspend now.
                            lock->m_awaiter = std::move(handler);
                            return;
                        }
                    }
                }
            };
            return asio::async_initiate<Token, void(async_mutex_noscope)>(initiation, handler);
        }

        /// \brief
        /// Acquire a lock on the mutex asynchronously, returning an object that
        /// will call unlock() automatically when it goes out of scope.
        ///
        /// If the lock could not be acquired synchronously then the awaiting
        /// coroutine will be suspended and later resumed when the lock becomes
        /// available. If suspended, the coroutine will be resumed inside the
        /// call to unlock() from the previous lock owner.
        ///
        /// \return
        /// An operation object that must be 'co_await'ed to wait until the
        /// lock is acquired. The result of the 'co_await m.scoped_lock_async()'
        /// expression returns an 'async_mutex_lock' object that will call
        /// this->mutex() when it destructs.
        template<class Token>
        auto scoped_lock_async(Token&& handler) { //FIX me segfault
            return lock_async(std::forward<Token>(handler), true);
        }
        /// \brief
        /// Unlock the mutex.
        ///
        /// Must only be called by the current lock-holder.
        ///
        /// If there are lock operations waiting to acquire the
        /// mutex then the next lock operation in the queue will
        /// be resumed inside this call.
        void unlock();

    private:

        friend class async_mutex_lock_operation;

        static constexpr std::uintptr_t not_locked = 1;

        // assume == reinterpret_cast<std::uintptr_t>(static_cast<void*>(nullptr))
        static constexpr std::uintptr_t locked_no_waiters = 0;

        // This field provides synchronisation for the mutex.
        //
        // It can have three kinds of values:
        // - not_locked
        // - locked_no_waiters
        // - a pointer to the head of a singly linked list of recently
        //   queued async_mutex_lock_operation objects. This list is
        //   in most-recently-queued order as new items are pushed onto
        //   the front of the list.
        std::atomic<std::uintptr_t> m_state;

        // Linked list of async lock operations that are waiting to acquire
        // the mutex. These operations will acquire the lock in the order
        // they appear in this list. Waiters in this list will acquire the
        // mutex before waiters added to the m_newWaiters list.
        async_mutex_lock_operation* m_waiters;

    };

    }
}

#endif // ASYNC_MUTEX_H
