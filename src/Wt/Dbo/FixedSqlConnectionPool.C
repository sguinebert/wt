/*
 * Copyright (C) 2010 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include "Wt/Dbo/Exception.h"
#include "Wt/Dbo/FixedSqlConnectionPool.h"
#include "Wt/Dbo/Logger.h"
#include "Wt/Dbo/SqlConnection.h"
#include "Wt/Dbo/StringStream.h"

#ifdef WT_THREADED
#include <thread>
#include <mutex>
#include <condition_variable>
#endif // WT_THREADED

#include <iostream>

#include <Wt/Dbo/backend/Postgres.h>
#include <Wt/Dbo/backend/Sqlite3.h>
#include <Wt/Dbo/backend/MySQL.h>
#include <Wt/Dbo/backend/MSSQLServer.h>
#include <Wt/Dbo/backend/Firebird.h>

namespace Wt
{
  namespace Dbo
  {

    LOGGER("Dbo.FixedSqlConnectionPool");

    struct FixedSqlConnectionPool::Impl
    {
#ifdef WT_THREADED
      std::mutex mutex;
      std::condition_variable connectionAvailable;
#endif // WT_THREADED

      std::chrono::steady_clock::duration timeout;
      std::vector<std::unique_ptr<SqlConnection>> freeList;
    };

    FixedSqlConnectionPool::FixedSqlConnectionPool(std::unique_ptr<SqlConnection> connection, int size)
        : impl_(new Impl)
    {
      SqlConnection *conn = connection.get();
      impl_->freeList.push_back(std::move(connection));

      for (int i = 1; i < size; ++i)
        impl_->freeList.push_back(std::visit([&] (auto& conn) -> std::unique_ptr<SqlConnection> { return std::make_unique<SqlConnection>(*conn.clone());  }, *conn));
    }

    FixedSqlConnectionPool::~FixedSqlConnectionPool()
    {
      impl_->freeList.clear();
    }

    void FixedSqlConnectionPool::setTimeout(std::chrono::steady_clock::duration timeout)
    {
      impl_->timeout = timeout;
    }

    std::chrono::steady_clock::duration FixedSqlConnectionPool::timeout() const
    {
      return impl_->timeout;
    }

    std::unique_ptr<SqlConnection> FixedSqlConnectionPool::getConnection()
    {
#ifdef WT_THREADED
      std::unique_lock<std::mutex> lock(impl_->mutex);

      while (impl_->freeList.empty())
      {
        LOG_WARN("no free connections, waiting for connection");
        fmtlog::poll();
        if (impl_->timeout > std::chrono::steady_clock::duration::zero())
        {
          if (impl_->connectionAvailable.wait_for(lock, impl_->timeout) == std::cv_status::timeout)
          {
            handleTimeout();
          }
        }
        else
          impl_->connectionAvailable.wait(lock);
      }
#else
      if (impl_->freeList.empty())
        throw Exception("FixedSqlConnectionPool::getConnection(): "
                        "no connection available but single-threaded build?");
#endif // WT_THREADED

      std::unique_ptr<SqlConnection> result = std::move(impl_->freeList.back());
      impl_->freeList.pop_back();

      return result;
    }

    void FixedSqlConnectionPool::handleTimeout()
    {
      throw Exception("FixedSqlConnectionPool::getConnection(): timeout");
    }

    void FixedSqlConnectionPool::returnConnection(std::unique_ptr<SqlConnection> connection)
    {
#ifdef WT_THREADED
      std::unique_lock<std::mutex> lock(impl_->mutex);
#endif // WT_THREADED

      impl_->freeList.push_back(std::move(connection));

#ifdef WT_THREADED
      if (impl_->freeList.size() == 1)
        impl_->connectionAvailable.notify_one();
#endif // WT_THREADED
    }

    void FixedSqlConnectionPool::prepareForDropTables() const
    {
      for (unsigned i = 0; i < impl_->freeList.size(); ++i)
            std::visit([&] (auto& conn) { conn.prepareForDropTables();  }, *impl_->freeList[i]); //impl_->freeList[i]->prepareForDropTables();
    }

  }
}
