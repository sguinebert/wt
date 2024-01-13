/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include <exception>
#include <iostream>

#include "Wt/Dbo/Exception.h"

#include "Wt/Dbo/Session.h"
#include "Wt/Dbo/SqlConnection.h"
//#include "Wt/Dbo/backend/connection.hpp"//#include "Wt/Dbo/SqlConnection.h"
#include "Wt/Dbo/StringStream.h"
#include "Wt/Dbo/Transaction.h"
#include "Wt/Dbo/ptr.h"
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

/*
 * About noexcept(false), see
 * http://akrzemi1.wordpress.com/2011/09/21/destructors-that-throw/
 */
Transaction::~Transaction() noexcept(false)
{
  // Either this Transaction shell was not committed (first condition)
  // or the commit failed (we are still active and need to rollback)
  if (!committed_ || impl_->needsRollback_) {
    // A commit attempt failed (and thus we need to rollback) or we
    // are unwinding a stack while an exception is thrown
    if (impl_->needsRollback_ || std::uncaught_exceptions()) {
      bool canThrow = std::uncaught_exceptions() == 0;
      try {
          impl_->rollback(true);
      }
      catch (...) {
        release();
        if (canThrow)
          throw;
      }
    }
    else {
      try {
        impl_->commit(true);
      }
      catch (...) {
        try {
          if (impl_->transactionCount_ == 1)
            impl_->rollback(true);
        }
        catch (std::exception &e) {
          LOG_ERROR("Unexpected exception during Transaction::rollback(): {}", e.what());
          fmtlog::poll();
        }
        catch (...) {
          LOG_ERROR("Unexpected exception during Transaction::rollback()");
          fmtlog::poll();
        }

        //release();
        throw;
      }
    }
  }
  //release();
}

void Transaction::release()
{
  --impl_->transactionCount_;

  if (impl_->transactionCount_ == 0){
    delete impl_;
  }
}

bool Transaction::isActive() const
{
  return impl_->active_;
}

bool Transaction::commit(bool)
{
  if (isActive()) {
    committed_ = true;

    if (impl_->transactionCount_ == 1) {
      impl_->commit(true);

      return true;
    } else
      return false;
  }
  return false;
}

awaitable<bool> Transaction::commit()
{
  if (isActive()) {
    committed_ = true;

    if (impl_->transactionCount_ == 1) {
      co_await impl_->commit();

      co_return true;
    } else
      co_return false;
  }
  co_return false;
}

awaitable<bool> Transaction::rollback()
{
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
  co_await impl_->open();
  //co_return impl_->connection_.get();
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

awaitable<void> Transaction::Impl::open()
{
  if (!open_) {
    open_ = true;

    co_await connection_->startTransaction();
//#ifndef VARIANT
//    auto tx = co_await connection_->async_transaction(use_awaitable);
//#else
//    auto tx = co_await std::visit([] (auto& conn) ->awaitable<postgrespp::work> { co_return co_await conn.startTransaction(); }, *connection_);
//#endif
//    transaction_ = std::make_shared<postgrespp::work>(std::move(tx));
  }
  co_return;
}

awaitable<void> Transaction::Impl::commit()
{
  needsRollback_ = true;
  if (session_.flushMode() == FlushMode::Auto)
    co_await session_.flush();

  if (open_) {
    co_await connection_->commitTransaction();
#ifndef VARIANT
    //co_await transaction_->commit(use_awaitable);
#else
    co_await std::visit([] (auto& conn) ->awaitable<void> { co_await conn.commitTransaction(); }, *connection_);
#endif
  }

  for (unsigned i = 0; i < objects_.size(); ++i) {
    objects_[i]->transactionDone(true);
    delete objects_[i];
  }

  objects_.clear();

  //session_.returnConnection(std::move(connection_));
  connection_ = nullptr;
  session_.transaction_ = nullptr;
  active_ = false;
  needsRollback_ = false;
  co_return;
}

awaitable<void> Transaction::Impl::rollback()
{
  needsRollback_ = false;

  try {
    if (open_) {
      co_await connection_->rollbackTransaction();
#ifndef VARIANT
      //co_await transaction_->rollback(use_awaitable);
#else
      co_await std::visit([] (auto& conn)->awaitable<void> {co_await conn.rollbackTransaction(); }, *connection_);
#endif
      //

    }
  } catch (const std::exception& e) {
    LOG_ERROR("Transaction::rollback(): {}", e.what());
    fmtlog::poll();
  }

  for (unsigned i = 0; i < objects_.size(); ++i) {
    objects_[i]->transactionDone(false);
    delete objects_[i];
  }

  objects_.clear();


  //session_.returnConnection(std::move(connection_));
  connection_ = nullptr;
  session_.transaction_ = nullptr;
  active_ = false;
  co_return;
}

void Transaction::Impl::commit(bool)
{
  needsRollback_ = true;
  if (session_.flushMode() == FlushMode::Auto)
    session_.flush([this] () {
        try {
            if (open_) {
                co_spawn(connection_->get_executor(), connection_->commitTransaction(), [this] (auto &&ec) {
                    --this->transactionCount_;

                    if (this->transactionCount_ == 0){
                        delete this;
                    }
                });
                //transaction_->commit(cb);
//                transaction_->commit([this, tr = transaction_] (auto &&res) {
//                    --this->transactionCount_;

//                    if (this->transactionCount_ == 0){
//                        delete this;
//                    }
//                });
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Transaction::rollback(): {}", e.what());
            fmtlog::poll();
        }

        for (unsigned i = 0; i < objects_.size(); ++i) {
            objects_[i]->transactionDone(false);
            delete objects_[i];
        }

        objects_.clear();

        //session_.returnConnection(std::move(connection_));
        connection_ = nullptr;
        session_.transaction_ = nullptr;
        active_ = false;

    });

}

void Transaction::Impl::rollback(bool)
{
  needsRollback_ = false;

  try {
    if (open_) {
      co_spawn(connection_->get_executor(), connection_->rollbackTransaction(), [this] (auto &&ec) {
          --this->transactionCount_;

          if (this->transactionCount_ == 0){
              delete this;
          }
      });
//      transaction_->rollback([this, tr = transaction_] (auto &&res) {
//          --this->transactionCount_;

//          if (this->transactionCount_ == 0){
//              delete this;
//          }
//      });
    }
  } catch (const std::exception& e) {
    LOG_ERROR("Transaction::rollback(): {}", e.what());
    fmtlog::poll();
  }

  for (unsigned i = 0; i < objects_.size(); ++i) {
    objects_[i]->transactionDone(false);
    delete objects_[i];
  }

  objects_.clear();


  //session_.returnConnection(std::move(connection_));
  connection_ = nullptr;
  session_.transaction_ = nullptr;
  active_ = false;
}

  }
}
