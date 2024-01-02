#include "async_mutex.h"

#include <cassert>

namespace Wt {
namespace cpp20 {

async_mutex::async_mutex() noexcept
    : m_state(not_locked)
    , m_waiters(nullptr)
{}

async_mutex::~async_mutex()
{
    [[maybe_unused]] auto state = m_state.load(std::memory_order_relaxed);
    assert(state == not_locked || state == locked_no_waiters);
    assert(m_waiters == nullptr);
}

bool async_mutex::try_lock() noexcept
{
    // Try to atomically transition from nullptr (not-locked) -> this (locked-no-waiters).
    auto oldState = not_locked;
    return m_state.compare_exchange_strong(
        oldState,
        locked_no_waiters,
        std::memory_order_acquire,
        std::memory_order_relaxed);
}

void async_mutex::unlock()
{
    assert(m_state.load(std::memory_order_relaxed) != not_locked);

    async_mutex_lock_operation* waitersHead = m_waiters;
    if (waitersHead == nullptr)
    {
        auto oldState = locked_no_waiters;
        const bool releasedLock = m_state.compare_exchange_strong(
            oldState,
            not_locked,
            std::memory_order_release,
            std::memory_order_relaxed);
        if (releasedLock)
        {
            return;
        }

        // At least one new waiter.
        // Acquire the list of new waiter operations atomically.
        oldState = m_state.exchange(locked_no_waiters, std::memory_order_acquire);

        assert(oldState != locked_no_waiters && oldState != not_locked);

        // Transfer the list to m_waiters, reversing the list in the process so
        // that the head of the list is the first to be resumed.
        auto* next = reinterpret_cast<async_mutex_lock_operation*>(oldState);
        do
        {
            auto* temp = next->m_next;
            next->m_next = waitersHead;
            waitersHead = next;
            next = temp;
        } while (next != nullptr);
    }

    assert(waitersHead != nullptr);

    m_waiters = waitersHead->m_next;

    // Resume the waiter.
    // This will pass the ownership of the lock on to that operation/coroutine.
    //waitersHead->m_awaiter();
    asio::dispatch(*waitersHead->m_context,  [this, waitersHead] () {
        waitersHead->m_awaiter(waitersHead->m_scopelocked ? async_mutex_lock{*this, std::adopt_lock } : async_mutex_noscope());
    });
}

async_mutex_lock::~async_mutex_lock()
{
    if (m_mutex != nullptr)
    {
        m_mutex->unlock();
    }
}
}
}
