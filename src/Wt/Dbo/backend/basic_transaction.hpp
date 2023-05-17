#pragma once

#include "field_type.hpp"
#include "query.hpp"
#include "socket_operations.hpp"
#include "type_encoder.hpp"
#include "utility.hpp"

#include <cassert>
#include <tuple>
#include <iostream>

namespace postgrespp {

class basic_connection;

template <class RWT, class IsolationT>
class basic_transaction : public socket_operations<basic_transaction<RWT, IsolationT>> {
  friend class socket_operations<basic_transaction<RWT, IsolationT>>;
public:
  using query_t = query;
  using connection_t = ::postgrespp::basic_connection;
  using statement_name_t = std::string;

private:
  using result_t = typename socket_operations<basic_transaction<RWT, IsolationT>>::result_t;

public:
  basic_transaction(connection_t& c)
    : c_{c}
    , done_{false} {
  }

  basic_transaction(const basic_transaction&) = delete;
  basic_transaction(basic_transaction&& rhs) noexcept
    : c_{std::move(rhs.c_)}
    , done_{std::move(rhs.done_)} {
    rhs.done_ = true;
  }

  basic_transaction& operator=(const basic_transaction&) = delete;
  basic_transaction& operator=(basic_transaction&& rhs) noexcept {
    using std::swap;

    swap(c_, rhs.c_);
    swap(done_, rhs.done_);
  }

  /**
   * Destructor.
   * If neither \ref commit() nor \ref rollback() has been used, destructing
   * will do a sync rollback.
   */

  ~basic_transaction() {
    if (!done_) {
      std::cerr << "transaction not completed (netheir 'commit()' nor 'rollback()' has been used destructing : will do a sync rollback" << std::endl;
      //const result_t res{PQexec(connection().underlying_handle(), "ROLLBACK")};
      PQsendQuery(connection().underlying_handle(), "ROLLBACK");
      if (!PQpipelineSync(connection().underlying_handle()))
      {
          std::cerr << "PQpipelineSync not working " << std::endl;
          return;
      }
      PQconsumeInput(connection().underlying_handle());

      while (PQisBusy(connection().underlying_handle()) != 0) {

          /* wait here */

      }
      const result_t res{PQgetResult(connection().underlying_handle())};

//      while (PQisBusy(connection().underlying_handle()) != 0) {

//          /* wait here */

//      }
      const result_t res2{PQgetResult(connection().underlying_handle())};

      std::cout << "res : " << (int)res2.status() << std::endl;
      assert(result_t::status_t::COMMAND_OK == res.status() || res.sync());
    }
  }
  
  /// See \ref async_exec(query, handler, params) for more.
  template <class ResultCallableT>
  auto async_exec(const query_t& query, ResultCallableT&& handler) {
    return async_exec_2(query, std::forward<ResultCallableT>(handler), nullptr, nullptr, nullptr, 0);
  }

  /// See \ref async_exec_prepared(statement_name, handler, params) for more.
  template <class ResultCallableT>
  auto async_exec_prepared(const statement_name_t& statement_name, ResultCallableT&& handler) {
    return async_exec_prepared_2(statement_name, std::forward<ResultCallableT>(handler), nullptr, nullptr, nullptr, 0);
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
  auto async_exec(const query_t& query, ResultCallableT&& handler,
      Params&&... params) {
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

  /**
   * Execute a query asynchronously.
   * \p statement_name prepared statement name.
   * \ref async_exec_all(query, handler, params).
   * \p handler will be called once with the result.
   * \p params parameters to pass in the same order to $1, $2, ...
   *
   * This function must not be called again before the handler is called.
   */
  template <typename... Args>
  struct contains_int_pointer : std::disjunction<std::is_same<int*, Args>...> {};

  template <class ResultCallableT, class... Params>
  auto async_exec_prepared(const statement_name_t& statement_name,
      ResultCallableT&& handler, Params&&... params) {
    using namespace utility;

    if constexpr (contains_int_pointer<Params...>::value) {
      return async_exec_prepared_2(statement_name,
                                   std::forward<ResultCallableT>(handler), params...);
    }
    else {
      const auto value_holders = create_value_holders(params...);
      const auto value_arr = std::apply(
          [this](auto&&... args) { return value_array(args...); },
          value_holders);
      const auto size_arr = size_array(params...);
      const auto type_arr = type_array(params...);

      return async_exec_prepared_2(statement_name,
                                   std::forward<ResultCallableT>(handler), value_arr.data(),
                                   size_arr.data(),type_arr.data(), sizeof...(params));
    }
  }

//  template <class ResultCallableT, class... Params>
//  auto async_exec_prepared(const statement_name_t& statement_name,
//                           ResultCallableT&& handler, const char* const* value_arr,
//                           const int* size_arr, const int* type_arr, std::size_t num_values) {
//    return async_exec_prepared_2(statement_name,
//                                 std::forward<ResultCallableT>(handler), value_arr,
//                                 size_arr, type_arr, num_values);
//  }

  /**
   * Execute queries asynchronously.
   * Supports multiple queries in \p query, separated by ';' but does not
   * support parameter binding.
   * \p handler will be called once for each query and once more with an
   * empty result where \ref result.done() returns true.
   *
   * This function must not be called again before the handler is called
   * with a result where \ref result.done() returns true.
   */
  template <class ResultCallableT>
  auto async_exec_all(const query_t& query, ResultCallableT&& handler) {
    assert(!done_);

    const auto res = PQsendQuery(connection().underlying_handle(),
        query.c_str());

    if (res != 1) {
      throw std::runtime_error{
        "error executing query: " + std::string{connection().last_error_message()}};
    }

    return this->handle_exec_all(std::forward<ResultCallableT>(handler));
  }

  template <class ResultCallableT>
  auto commit(ResultCallableT&& handler) {
    const auto initiation = [this](auto&& handler) {
      async_exec("COMMIT", [this, handler = std::move(handler)](auto&& res) mutable {
          done_ = true;
          handler(std::forward<decltype(res)>(res));
        });
    };

    return boost::asio::async_initiate<
      ResultCallableT, void(result_t)>(
          initiation, handler);
  }

  template <class ResultCallableT>
  auto rollback(ResultCallableT&& handler) {
    const auto initiation = [this](auto&& handler) {
      async_exec("ROLLBACK", [this, handler = std::move(handler)](auto&& res) mutable {
          done_ = true;
          handler(std::forward<decltype(res)>(res));
        });
    };

    return boost::asio::async_initiate<
      ResultCallableT, void(result_t)>(
          initiation, handler);
  }

protected:
  auto& connection() { return c_.get(); }

  auto& socket() { return connection().socket(); }

private:

    template <class ResultCallableT>
    auto async_exec_2(const query_t& query, ResultCallableT&& handler,
                      const char* const* value_arr, const int* size_arr, const int* type_arr,
                      std::size_t num_values) {
    assert(!done_);

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

    std::cout << "query : " << query << std::endl;
    const auto res = PQsendQueryParams(connection().underlying_handle(),
                                       query.c_str(),
                                       num_values,
                                       nullptr,
                                       value_arr,
                                       size_arr,
                                       type_arr,
                                       static_cast<int>(field_type::BINARY));

    if (res != 1) {
      throw std::runtime_error{
        "error executing query '" + query + "': " + std::string{connection().last_error_message()}};
    }

    if(query == "BEGIN" || query == "COMMIT") {
      if (!PQpipelineSync(connection().underlying_handle()))
      {
          std::cerr << "pipeline start not working " << std::endl;
      }
    }

    return this->handle_exec(std::forward<ResultCallableT>(handler));
  }

  template <class ResultCallableT>
  auto async_exec_prepared_2(const statement_name_t& statement_name,
      ResultCallableT&& handler, const char* const* value_arr,
      const int* size_arr, const int* type_arr, std::size_t num_values) {
    assert(!done_);

    const auto res = PQsendQueryPrepared(connection().underlying_handle(),
        statement_name.c_str(),
        num_values,
        value_arr,
        size_arr,
        type_arr,
        1);

    if (res != 1) {
      throw std::runtime_error{
        "error executing query '" + statement_name + "': " + std::string{connection().last_error_message()}};
    }

    return this->handle_exec(std::forward<ResultCallableT>(handler));
  }

private:
  std::reference_wrapper<connection_t> c_;
  bool done_;

  friend class basic_connection;
};

}
