/*
 * Copyright (C) 2010 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include "Wt/Dbo/Exception.h"
#include "Wt/Dbo/FixedSqlConnectionPool.h"
#include "Wt/Dbo/SqlConnection.h"
//#include "Wt/Dbo/backend/connection.hpp"//#include "Wt/Dbo/SqlConnection.h"
#include "Wt/Dbo/StringStream.h"

#ifdef WT_THREADED
#include <thread>
#include <mutex>
#include <condition_variable>
#endif // WT_THREADED

#include <iostream>


namespace Wt
{
  namespace Dbo
  {

    //LOGGER("Dbo.FixedSqlConnectionPool");

    struct FixedSqlConnectionPool::Impl
    {
#ifdef WT_THREADED
      std::mutex mutex;
      std::condition_variable connectionAvailable;
#endif // WT_THREADED

      std::chrono::steady_clock::duration timeout;
      std::vector<std::unique_ptr<SqlConnection>> freeList;

      static inline thread_local SqlConnection* connection = nullptr;
    };

    FixedSqlConnectionPool::FixedSqlConnectionPool(std::unique_ptr<SqlConnection> connection,
                                                   unsigned size, Wt::http::detail::engines& engine)
        : impl_(new Impl), engine_(engine)
    {
      SqlConnection *conn = connection.get();
      impl_->freeList.push_back(std::move(connection));

//      for (unsigned i = 1; i < size; ++i)
//        impl_->freeList.push_back(conn->clone());


#ifndef VARIANT
      conn->setCancelSignal(&cancel_signal_);
#else
        std::visit([&] (auto& conn) { conn.setCancelSignal(&cancel_signal_); }, *conn);
#endif



      for (unsigned i = 1; i < size; ++i) {

#ifndef VARIANT
          impl_->freeList.push_back(conn->clone(engine.get()));
          impl_->freeList.back()->setCancelSignal(&cancel_signal_);

          //init thread_local connection pointer of each context if available
          asio::post(impl_->freeList.back()->get_executor(), [this] { this->impl_->connection = impl_->freeList.back().get(); });

          //asio::post(impl_->freeList.back()->socket().get_executor(), [this] { this->impl_->connection = impl_->freeList.back().get(); });
#else
        std::visit([&] (auto& conn) {
              auto cv = std::make_unique<SqlConnection>(conn.clone(engine));
              impl_->freeList.push_back(std::move(cv));
              conn.setCancelSignal(&cancel_signal_);
              asio::post(conn.socket().get_executor(), [&, this] {
                this->impl_->connection = impl_->freeList.back().get();
              });
          }, *impl_->freeList.back());
#endif
      }
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
        //LOG_WARN("no free connections, waiting for connection");
        //fmtlog::poll();
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

    SqlConnection* FixedSqlConnectionPool::get_rconnection() {
        if(impl_->connection) {
          return impl_->connection;
        }
        for(unsigned i = 0; i < impl_->freeList.size(); i++)
        {
          if(increment++; increment >= impl_->freeList.size())
            increment = 0;
          auto& c = impl_->freeList[increment];
          return c.get();
        }
        return nullptr;
    }

    awaitable<SqlConnection *> FixedSqlConnectionPool::async_connection(bool transaction)
    {
      while (true) {
          if(impl_->connection &&
#ifndef VARIANT
              !impl_->connection->inTransaction(transaction)
#else
              std::visit([&] (auto& conn) ->bool { return conn.inTransaction(transaction); }, *impl_->connection)
#endif
              ) {
            co_return impl_->connection;
          }
          for(unsigned i = 0; i < impl_->freeList.size(); i++)
          {
            if(increment++; increment >= impl_->freeList.size())
                increment = 0;
            auto& c = impl_->freeList[increment];
            if(
#ifndef VARIANT
                !c->inTransaction(transaction)
#else
                std::visit([&] (auto& conn) ->bool { return conn.inTransaction(transaction); }, *c)
#endif
                ) {
              co_return c.get();
            }
          }
          auto timer = asio::steady_timer(co_await asio::this_coro::executor, std::chrono::microseconds::max());
          co_await timer.async_wait(asio::bind_cancellation_slot(cancel_signal_.slot(), use_nothrow_awaitable));
      }
      handleTimeout();
      co_return nullptr;
    }

    void FixedSqlConnectionPool::async_connection(bool transaction, std::function<void (SqlConnection *)> cb)
    {

      //while (true) {
      if(impl_->connection &&
#ifndef VARIANT
          !impl_->connection->inTransaction(transaction)
#else
          std::visit([&] (auto& conn) ->bool { return conn.inTransaction(transaction); }, *impl_->connection)
#endif
          ) {
            cb(impl_->connection);
            return;
          }
          for(auto& c : impl_->freeList)
          {
            if(
#ifndef VARIANT
                !c->inTransaction(transaction)
#else
                std::visit([&] (auto& conn) ->bool { return conn.inTransaction(transaction); }, *c)
#endif
                ) {
              cb(c.get());
              return;
            }
          }


          auto timer = asio::steady_timer(*thread_context, std::chrono::microseconds::max());
          timer.async_wait(asio::bind_cancellation_slot(cancel_signal_.slot(), [this, transaction, cb = std::move(cb)] (auto ec) {
              async_connection(transaction, std::move(cb));
          }));

//          auto timer = asio::steady_timer(co_await asio::this_coro::executor, std::chrono::microseconds::max());
//          timer.async_wait(asio::bind_cancellation_slot(cancel_signal_.slot(), use_nothrow_awaitable));
      //}
      //handleTimeout();
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
#ifndef VARIANT
        impl_->freeList[i]->prepareForDropTables();
#else
      std::visit([&] (auto& conn) { conn.prepareForDropTables();  }, *impl_->freeList[i]);
#endif
    }

  }
}
