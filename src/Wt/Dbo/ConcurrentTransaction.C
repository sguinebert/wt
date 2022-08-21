/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include <exception>
#include <iostream>

#include "Wt/Dbo/Logger.h"
#include "Wt/Dbo/Session.h"
#include "Wt/Dbo/SqlConnection.h"
#include "Wt/Dbo/StringStream.h"
#include "Wt/Dbo/ConcurrentTransaction.h"
#include "Wt/Dbo/Transaction.h"
#include "Wt/Dbo/ptr.h"
#include "Wt/Dbo/Exception.h"

namespace Wt {
  namespace Dbo {

LOGGER("Dbo.ConcurrentTransaction");

ConcurrentTransaction::ConcurrentTransaction(Session& session)
  : committed_(false),
    session_(session)
{
  Transaction::Impl *transaction(nullptr);

  auto id = std::this_thread::get_id();

  // if(!session_.transactions_.contains(id))
  //   throw std::exception("no concurrent thread register");
  //session_.transactions_.find(id, transaction);
    
  if (auto search = session_.transactions_.find(id); search != session_.transactions_.end())
  {
    transaction = search->second;
  }
  else
    throw Exception("Thread not find : the thread need to be register to dbo::session");



  if (!transaction) {
    impl_ = new Transaction::Impl(session_);
    session_.transactions_.at(id) = impl_;
  }
  else {
    impl_ = transaction;
  }
  ++impl_->transactionCount_;

  //if (!session_.transaction_)
  //  session_.transaction_ = new Impl(session_);

  //impl_ = session_.transaction_;

  //++impl_->transactionCount_;
}

/*
 * About noexcept(false), see
 * http://akrzemi1.wordpress.com/2011/09/21/destructors-that-throw/
 */
ConcurrentTransaction::~ConcurrentTransaction() noexcept(false)
{
  // Either this Transaction shell was not committed (first condition)
  // or the commit failed (we are still active and need to rollback)
  if (!committed_ || impl_->needsRollback_) {
    // A commit attempt failed (and thus we need to rollback) or we
    // are unwinding a stack while an exception is thrown
     if (impl_->needsRollback_ || std::uncaught_exceptions()) {
      bool canThrow = std::uncaught_exceptions() == 0;
      try {
    rollback();
      } catch (...) {
    release();
    if (canThrow)
      throw;
      }
    } else {
      try {
    commit();
      } catch (...) {
    try {
      if (impl_->transactionCount_ == 1)
        rollback();
        } catch (std::exception &e) {
          LOG_ERROR("Unexpected exception during Transaction::rollback(): {}", e.what());
    } catch (...) {
      LOG_ERROR("Unexpected exception during Transaction::rollback()");
    }

    release();
    throw;
      }
    }
  }

  release();
}

void ConcurrentTransaction::release()
{
  --impl_->transactionCount_;

  if (impl_->transactionCount_ == 0) {
    session_.transactions_.at(std::this_thread::get_id()) = nullptr;
    delete impl_;
  }
}

bool ConcurrentTransaction::isActive() const
{
  return impl_->active_;
}

bool ConcurrentTransaction::commit()
{
  if (isActive()) {
    committed_ = true;

    if (impl_->transactionCount_ == 1) {
      impl_->commit();

      return true;
    } else
      return false;
  } else
    return false;
}

void ConcurrentTransaction::rollback()
{
  if (isActive())
    impl_->rollback();
}

Session& ConcurrentTransaction::session() const
{
  return session_;
}

SqlConnection *ConcurrentTransaction::connection() const
{
  impl_->open();
  return impl_->connection_.get();
}

// Transaction::Impl::Impl(Session& session)
//   : session_(session),
//     active_(true),
//     needsRollback_(false),
//     open_(false),
//     transactionCount_(0)
// {
//   connection_ = session_.useConnection();
// }

// ConcurrentTransaction::Impl::~Impl()
// {
//   if (connection_)
//     session_.returnConnection(std::move(connection_));
// }

// void ConcurrentTransaction::Impl::open()
// {
//   if (!open_) {
//     open_ = true;
//     connection_->startTransaction();
//   }
// }

// void ConcurrentTransaction::Impl::commit()
// {
//   needsRollback_ = true;
//   if (session_.flushMode() == FlushMode::Auto)
//     session_.flush();

//   if (open_)
//     connection_->commitTransaction();

//   for (unsigned i = 0; i < objects_.size(); ++i) {
//     objects_[i]->transactionDone(true);
//     delete objects_[i];
//   }

//   objects_.clear();

//   session_.returnConnection(std::move(connection_));
//   auto id = std::this_thread::get_id();
//   session_.transactions_.at(id) = nullptr;
//   //auto it = session_.transactions_.find(id_);
//   //session_.transactions_.erase(id_);
//   //session_.transaction_ = nullptr;
//   active_ = false;
//   needsRollback_ = false;
// }

// void ConcurrentTransaction::Impl::rollback()
// {
//   needsRollback_ = false;

//   try {
//     if (open_)
//       connection_->rollbackTransaction();
//   } catch (const std::exception& e) {
//     LOG_ERROR("Transaction::rollback(): {}", e.what());
//   }

//   for (unsigned i = 0; i < objects_.size(); ++i) {
//     objects_[i]->transactionDone(false);
//     delete objects_[i];
//   }

//   objects_.clear();


//   session_.returnConnection(std::move(connection_));
//   auto id = std::this_thread::get_id();
//   session_.transactions_.at(id) = nullptr;
//   //auto it = session_.transactions_.find(id_);
//   //session_.transactions_.erase(id_);
//   //session_.transaction_ = nullptr;
//   active_ = false;
// }

  }
}
