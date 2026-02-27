/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include <iostream>
#include <exception>

#include "Wt/Dbo/session/Session.h"
#include "Wt/Dbo/sql/Connection.h"
//#include "Wt/Dbo/backend/connection.hpp"//#include "Wt/Dbo/sql/Connection.h"
#include "Wt/Dbo/session/Transaction.h"
#include "Wt/Dbo/Logger.h"

namespace Wt {
  namespace Dbo {

//LOGGER("Dbo.Transaction");

Transaction::Transaction(Session& session)
  : committed_(false),
    session_(session)
{
  if (!session_.transaction_)
    session_.transaction_ = new Impl(session_);

  impl_ = session_.transaction_;

  ++impl_->transactionCount_;
}

Transaction::Transaction(Transaction&& other) noexcept
  : committed_(other.committed_),
    session_(other.session_),
    impl_(other.impl_)
{
  other.committed_ = true;
  other.impl_ = nullptr;
}

Transaction::~Transaction() noexcept
{
  if (!impl_)
    return;

  if (!committed_ && impl_->active_) {
    LOG_ERROR("Transaction destroyed without explicit co_await commit() — rolling back");
    fmtlog::poll();

    if (impl_->transactionCount_ == 1) {
      impl_->rollback_detached(); // impl_ takes ownership of itself, will self-delete
      return;                     // skip release() — impl_ manages its own lifetime now
    }
  }

  release();
}

void Transaction::release()
{
  if (!impl_)
    return;

  --impl_->transactionCount_;

  if (impl_->transactionCount_ == 0){
    delete impl_;
  }
}

bool Transaction::isActive() const
{
  return impl_ && impl_->active_;
}

awaitable<bool> Transaction::commit()
{
  if (!impl_)
    co_return false;

  if (isActive()) {
    committed_ = true;

    if (impl_->transactionCount_ == 1) {
      auto r = co_await impl_->commit();
      if (!r) {
        LOG_ERROR("Transaction::commit(): {}", to_string(r.error()));
        fmtlog::poll();
      }

      co_return true;
    } else
      co_return false;
  }
  co_return false;
}

awaitable<bool> Transaction::rollback()
{
  if (!impl_)
    co_return false;

  if (isActive())
    co_await impl_->rollback();
  co_return true;
}

Session& Transaction::session() const
{
  return session_;
}

awaitable<SqlConnection *> Transaction::connection() const
{
  if (!impl_)
    co_return nullptr;

  auto r = co_await impl_->open();
  if (!r) {
    LOG_ERROR("Transaction::connection(): open failed: {}", to_string(r.error()));
    fmtlog::poll();
  }
  co_return impl_->connection_;
}

Transaction::Impl::Impl(Session& session)
  : session_(session),
    active_(true),
    needsRollback_(false),
    open_(false),
    transactionCount_(0)
{
  //connection_ = session_.useConnection();
}

awaitable<void> Transaction::Impl::assign_connection() {
  connection_ = co_await session_.assign_connection(true);
}

Transaction::Impl::~Impl()
{
//  if (connection_)
//    session_.returnConnection(std::move(connection_));
    session_.transaction_ = nullptr;
}

awaitable<dbo_result<void>> Transaction::Impl::open()
{
  if (!open_) {
    open_ = true;

    auto r = co_await connection_->startTransaction();
    if (!r) {
      open_ = false;
      co_return std::unexpected(r.error());
    }
  }
  co_return dbo_result<void>{};
}

awaitable<dbo_result<void>> Transaction::Impl::commit()
{
  needsRollback_ = true;
  if (session_.flushMode() == FlushMode::Auto)
    co_await session_.flush();

  if (open_) {
    auto r = co_await connection_->commitTransaction();
    if (!r) {
      co_return std::unexpected(r.error());
    }
  }

  connection_ = nullptr;
  session_.transaction_ = nullptr;
  active_ = false;
  needsRollback_ = false;
  co_return dbo_result<void>{};
}

awaitable<void> Transaction::Impl::rollback()
{
  needsRollback_ = false;

  if (open_) {
    auto r = co_await connection_->rollbackTransaction();
    if (!r) {
      LOG_ERROR("Transaction::rollback(): {}", to_string(r.error()));
      fmtlog::poll();
    }
  }

  connection_ = nullptr;
  session_.transaction_ = nullptr;
  active_ = false;
  co_return;
}

// Called only when transactionCount_ == 1 — this Impl takes ownership of itself.
// The caller (destructor) must NOT call release() afterwards.
void Transaction::Impl::rollback_detached()
{
  needsRollback_ = false;

  if (open_) {
    co_spawn(connection_->get_executor(), connection_->rollbackTransaction(),
      [this](std::exception_ptr ep, dbo_result<void> r) {
        if (ep) {
          LOG_ERROR("Transaction::rollback_detached() coroutine raised exception");
        } else if (!r) {
          LOG_ERROR("Transaction::rollback_detached(): {}", to_string(r.error()));
        }
        connection_ = nullptr;
        session_.transaction_ = nullptr;
        active_ = false;
        delete this;
      });
  } else {
    connection_ = nullptr;
    session_.transaction_ = nullptr;
    active_ = false;
    delete this;
  }
}

  }
}
