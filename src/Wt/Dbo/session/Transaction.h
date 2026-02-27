// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#ifndef WT_DBO_TRANSACTION_H_
#define WT_DBO_TRANSACTION_H_

#include <memory>
#include <vector>
#include <Wt/Dbo/WDboDllDefs.h>
#include <Wt/Dbo/sql/Connection.h>

namespace Wt {
  namespace Dbo {

class Session;

/*! \class Transaction Wt/Dbo/Transaction.h Wt/Dbo/Transaction.h
 *  \brief A database transaction.
 *
 * This class implements a transaction with explicit async commit.
 * The commit MUST be co_awaited explicitly. If the transaction is
 * destroyed without a commit, it will rollback (fire-and-forget)
 * and log an error.
 *
 * Usage example (style A — explicit commit):
 * \code
 * awaitable<void> doSomething(Wt::Dbo::Session& session)
 * {
 *   auto t = co_await session.transaction();
 *   auto a = co_await session.load<Account>(42);
 *   // ...
 *   co_await t.commit();
 * }
 * \endcode
 *
 * Usage example (style B — with_transaction wrapper):
 * \code
 * auto tx = co_await session.with_transaction([&]() -> awaitable<dbo_result<void>> {
 *   auto a = co_await session.load<Account>(42);
 *   if (!a)
 *     co_return std::unexpected(a.error());
 *   // ... commit is automatic
 *   co_return {};
 * });
 * \endcode
 *
 * \ingroup dbo
 */
class WTDBO_API Transaction
{
public:
  /*! \brief Constructor.
   *
   * Opens a transaction for the given \p session. If a transaction is
   * already open for the session, this transaction is added. All open
   * transactions must commit successfully for the entire transaction to
   * succeed.
   */
  explicit Transaction(Session& session);

  /*! \brief Destructor.
   *
   * If the transaction is still active, it is rolled back
   * (fire-and-forget) and an error is logged.
   */
  ~Transaction() noexcept;

  Transaction(const Transaction&) = delete;
  Transaction& operator=(const Transaction&) = delete;

  Transaction(Transaction&& other) noexcept;
  Transaction& operator=(Transaction&&) = delete;

  /*! \brief Returns whether the transaction is still active.
   *
   * A transaction is active unless it has been committed or rolled
   * back.
   *
   * While a transaction is active, new transactions for the same
   * session are treated as nested transactions.
   */
  bool isActive() const;

  /*! \brief Commits the transaction.
   *
   * If this is the last open transaction for the session, the session
   * is flushed and pending changes are committed to the database.
   *
   * Returns whether the transaction was flushed to the database
   * (i.e. whether this was indeed the last open transaction).
   *
   * \sa rollback()
   */
  [[nodiscard]] awaitable<bool> commit();

  /*! \brief Rolls back the transaction.
   *
   * The transaction is rolled back (if it was still active), and is no
   * longer active.
   *
   * \sa commit()
   */
  awaitable<bool> rollback();

  /*! \brief Returns the session associated with this transaction.
   *
   * \sa Transaction()
   */
  Session& session() const;

  /*! \brief Returns the connection used by this transaction
   */
  awaitable<SqlConnection *> connection() const;

private:
  struct Impl {
    Session& session_;
    bool active_;
    bool needsRollback_;
    bool open_;

    int transactionCount_;

    SqlConnection* connection_ = nullptr;

    awaitable<dbo_result<void>> open();
    awaitable<dbo_result<void>> commit();
    awaitable<void> rollback();
    void rollback_detached();

    Impl(Session& session_);
    awaitable<void> assign_connection();
    ~Impl();
  };

  bool committed_;
  Session& session_;
  Impl *impl_;

  friend class Session;
  friend class ConcurrentTransaction;

  void release();
};

  }
}

#endif // WT_DBO_TRANSACTION_H_
