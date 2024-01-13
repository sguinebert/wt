// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2010 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#ifndef WT_DBO_SQL_CONNECTION_POOL_H_
#define WT_DBO_SQL_CONNECTION_POOL_H_

#include <Wt/Dbo/WDboDllDefs.h>
#include <Wt/Dbo/SqlConnection.h>

#include <memory>
#include <vector>

namespace Wt {
  namespace Dbo {

//class sqlConnection;

/*! \class SqlConnectionPool Wt/Dbo/SqlConnectionPool.h Wt/Dbo/SqlConnectionPool.h
 *  \brief Abstract base class for a SQL connection pool.
 *
 * An sql connection pool manages a pool of connections. It is shared
 * between multiple sessions to allow these sessions to use a
 * connection while handling a transaction. Note that a session only
 * needs a connection while in-transaction, and thus you only need as
 * much connections as the number of concurrent transactions.
 *
 * \ingroup dbo
 */
class WTDBO_API SqlConnectionPool
{
public:
  /*! \brief Destructor.
   */
  virtual ~SqlConnectionPool();

  /*! \brief Uses a connection from the pool.
   *
   * This returns a connection from the pool that can be used. If the
   * pool has no more connection available, the pool may decide to
   * grow or block until a connection is returned.
   *
   * This method is called by a Session when a new transaction is
   * started.
   */
  virtual std::unique_ptr<sqlConnection> getConnection() = 0;

  /*! \brief Returns a connection to the pool.
   *
   * This returns a connection to the pool. This method is called by a
   * Session after a transaction has been finished.
   */
  virtual void returnConnection(std::unique_ptr<sqlConnection>) = 0;
  
  /*! \brief Prepares all connections in the pool for dropping the tables.
   */
  virtual void prepareForDropTables() const = 0;

  virtual awaitable<sqlConnection*> async_connection(bool transaction = false) = 0;

  virtual void async_connection(bool transaction, std::function<void(sqlConnection*)> cb) = 0;

  virtual sqlConnection* get_rconnection() = 0;
};

  }
}

#endif // WT_DBO_SQL_CONNECTION_POOL_H_
