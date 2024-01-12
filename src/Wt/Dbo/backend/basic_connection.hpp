#pragma once

#include "basic_transaction.hpp"
#include "query.hpp"
#include "socket_operations.hpp"
#include "utility.hpp"

#include <postgresql/libpq-fe.h>

//#include <boost/asio.hpp>
//#include <boost/asio/io_context.hpp>
//#include <boost/asio/ip/tcp.hpp>
#include <Wt/AsioWrapper/asio.hpp>

#include <stdexcept>
#include <string>
#include <utility>

#include <Wt/AsioWrapper/asio.hpp>

namespace postgrespp {

template <class, class>
class basic_transaction;

class result;

class basic_connection : public socket_operations<basic_connection>
{
  friend class socket_operations<basic_connection>;
public:
  using io_context_t = boost::asio::io_context;
  using result_t = result;
  using socket_t = boost::asio::ip::tcp::socket;
  using query_t = query;
  using statement_name_t = std::string;

public:
//  basic_connection(const char* const& pgconninfo)
//    : basic_connection{standalone_ioc(), pgconninfo} {
//  }

  template <class ExecutorT>
  basic_connection(ExecutorT& exc, const char* const& pgconninfo)
    : socket_{exc}
    {
    c_ = PQconnectdb(pgconninfo);

    if (status() != CONNECTION_OK)
      throw std::runtime_error{"could not connect: " + std::string{PQerrorMessage(c_)}};

    if (PQsetnonblocking(c_, 1) != 0)
      throw std::runtime_error{"could not set non-blocking: " + std::string{PQerrorMessage(c_)}};

    if (PQenterPipelineMode(c_) == 0)
      throw std::runtime_error{"could not set pipeline mode: " + std::string{PQerrorMessage(c_)}};

    const auto socket = PQsocket(c_);

    if (socket < 0)
      throw std::runtime_error{"could not get a valid descriptor"};

    struct sockaddr_storage addr;
    socklen_t len = sizeof(addr);
    getsockname(socket, (struct sockaddr *)&addr, &len);

    socket_.assign(addr.ss_family == AF_INET ? boost::asio::ip::tcp::v4() : boost::asio::ip::tcp::v6(), socket);
  }

  ~basic_connection() {
    if (c_)
      PQfinish(c_);
  }

  basic_connection(basic_connection const&) = delete;

  basic_connection(basic_connection&& rhs) noexcept
    : socket_{std::move(rhs.socket_)}
    , c_{std::move(rhs.c_)} {
    rhs.c_ = nullptr;
  }

  basic_connection& operator=(basic_connection const&) = delete;

  basic_connection& operator=(basic_connection&& rhs) noexcept {
    using std::swap;

    swap(socket_, rhs.socket_);
    swap(c_, rhs.c_);

    return *this;
  }

  template <class CompletionTokenT>
  auto async_prepare(const statement_name_t& statement_name,
                     const query_t& query,
                     int nParams,
                     Oid *paramTypes,
                     CompletionTokenT&& handler)
  {
    boost::asio::dispatch(socket().get_executor(), [this, statement_name = std::move(statement_name), query = std::move(query), nParams, paramTypes] {
        const auto res = PQsendPrepare(connection().underlying_handle(),
                                       statement_name.c_str(),
                                       query.c_str(),
                                       nParams,
                                       paramTypes);

        if (res != 1) {
            std::cerr << "error preparing statement '" + statement_name + "': " + std::string{connection().last_error_message()} << std::endl;
            throw std::runtime_error{"error preparing statement '" + statement_name + "': " + std::string{connection().last_error_message()}};
        }
    });

    return handle_exec(std::forward<CompletionTokenT>(handler));
  }

  /**
   * Creates a read/write transaction. Make sure the created transaction
   * object lives until you are done with it.
   */
  template <
    class Unused_RWT = void,
    class Unused_IsolationT = void,
    class TransactionHandlerT>
  auto async_transaction(TransactionHandlerT&& handler) {
    using txn_t = basic_transaction<Unused_RWT, Unused_IsolationT>;

    inTransaction_.store(true, std::memory_order_relaxed);

    auto initiation = [this](auto&& handler) {
      auto w = std::make_shared<txn_t>(*this); //, [this] {inTransaction_.store(false, std::memory_order_relaxed);}

      w->async_exec("BEGIN",
          [handler = std::move(handler), w](auto&& res) mutable { handler(std::move(*w)); } );
    };

    return boost::asio::async_initiate<TransactionHandlerT, void(txn_t)>(initiation, handler);
  }

  /**
   * execute without transaction. Make sure you don't execture several interdependant
   * SQL command that need rollback in case of failure
   */
  template <class ResultCallableT>
  auto async_exec(const query_t& query, ResultCallableT&& handler) {
    return async_exec_2(query, std::forward<ResultCallableT>(handler), nullptr, nullptr, nullptr, 0);
  }

  /**
   * Execute a query asynchronously.
   * \p query must contain a single query. For multiple queries, see
   * \ref async_exec_all(query, handler, params).
   * \p handler will be called once with the result.
   * \p params parameters to pass in the same order to $1, $2, ...
   *
   * This function must not be called again before the handler is called.
   */
  template <class ResultCallableT, class... Params>
  auto async_exec(const query_t& query, ResultCallableT&& handler, Params&&... params) {
    using namespace utility;

    const auto value_holders = create_value_holders(params...);
    const auto value_arr = std::apply(
        [this](auto&&... args) { return value_array(args...); },
        value_holders);
    const auto size_arr = size_array(params...);
    const auto type_arr = type_array(params...);

    return async_exec_2(query, std::forward<ResultCallableT>(handler),
                        value_arr.data(), size_arr.data(), type_arr.data(), sizeof...(params));
  }

  /// See \ref async_exec_prepared(statement_name, handler, params) for more.
  template <class ResultCallableT>
  auto async_exec_prepared(const statement_name_t& statement_name, ResultCallableT&& handler) {
    return async_exec_prepared_2(statement_name, std::forward<ResultCallableT>(handler), nullptr, nullptr, nullptr, 0);
  }

  /**
   * Execute a query asynchronously.
   * \p statement_name prepared statement name.
   * \ref async_exec_all(query, handler, params).
   * \p handler will be called once with the result.
   * \p params parameters to pass in the same order to $1, $2, ...
   *
   * This function must not be called again before the handler is called.
   */

  template <class ResultCallableT, class... Params>
  auto async_exec_prepared(const statement_name_t& statement_name,
                           ResultCallableT&& handler, Params&&... params) {
    using namespace utility;

    if constexpr (std::disjunction_v<std::is_same<int*, typename std::decay<Params>::type>...>) { //contains_int_pointer<Params...>
      return async_exec_prepared_2(statement_name,
                                   std::forward<ResultCallableT>(handler), params...);
    }
    else {
      const auto value_holders = create_value_holders(params...);
      const auto value_arr = std::apply(
          [](auto&&... args) { return value_array(args...); },
          value_holders);
      const auto size_arr = size_array(params...);
      const auto type_arr = type_array(params...);

      return async_exec_prepared_2(statement_name,
                                   std::forward<ResultCallableT>(handler), value_arr.data(),
                                   size_arr.data(),type_arr.data(), sizeof...(params));
    }
  }

  PGconn* underlying_handle() { return c_; }

  const PGconn* underlying_handle() const { return c_; }

  socket_t& socket() { return socket_; }

  //auto& executor() { return socket_.get_executor(); }

  const char* last_error_message() const { return PQerrorMessage(underlying_handle()); }

  bool inTransaction(bool transaction) { if(transaction)return inTransaction_.exchange(transaction, std::memory_order_relaxed);  return inTransaction_.load(std::memory_order_relaxed); }

private:
  int status() const {
      return PQstatus(c_);
  }

  template <class ResultCallableT>
  auto async_exec_2(const query_t& query, ResultCallableT&& handler,
                    const char* const* value_arr, const int* size_arr, const int* type_arr,
                    std::size_t num_values) {

      //    auto capture = [this, query = std::move(query), num_values, value_arr, size_arr, type_arr] () {
      //        const auto res = PQsendQueryParams(connection().underlying_handle(),
      //            query.c_str(),
      //            num_values,
      //            nullptr,
      //            value_arr,
      //            size_arr,
      //            type_arr,
      //            static_cast<int>(field_type::BINARY));

      //        if (res != 1) {
      //          throw std::runtime_error{
      //            "error executing query '" + query + "': " + std::string{connection().last_error_message()}};
      //        }
      //    };

      //    return this->handle_exec2(std::forward<ResultCallableT>(handler), std::move(capture));

      //std::cout << "query : " << query << std::endl;
      boost::asio::dispatch(socket().get_executor(), [this, query = std::move(query), num_values, value_arr, size_arr, type_arr] {
          const auto res = PQsendQueryParams(connection().underlying_handle(),
                                             query.c_str(),
                                             num_values,
                                             nullptr,
                                             value_arr,
                                             size_arr,
                                             type_arr,
                                             static_cast<int>(field_type::BINARY));

          if (res != 1) {
              throw std::runtime_error{"error executing query '" + query + "': " + std::string{connection().last_error_message()}};
          }

      });

      return this->handle_exec(std::forward<ResultCallableT>(handler));
  }

  template <class ResultCallableT>
  auto async_exec_prepared_2(const statement_name_t& statement_name,
                             ResultCallableT&& handler, const char* const* value_arr,
                             const int* size_arr, const int* type_arr, std::size_t num_values) {

      boost::asio::dispatch(socket().get_executor(), [this, statement_name = std::move(statement_name), num_values, value_arr, size_arr, type_arr] {
          const auto res = PQsendQueryPrepared(connection().underlying_handle(),
                                               statement_name.c_str(),
                                               num_values,
                                               value_arr,
                                               size_arr,
                                               type_arr,
                                               1);

          if (res != 1) {
              throw std::runtime_error{"error executing query '" + statement_name + "': " + std::string{connection().last_error_message()}};
          }
      });

      return this->handle_exec(std::forward<ResultCallableT>(handler));
  }

  basic_connection& connection() { return *this; }

  io_context_t& standalone_ioc();

private:
  socket_t socket_;

  PGconn* c_;

  std::atomic_bool inTransaction_ = false;
};

}
