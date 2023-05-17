#pragma once

#include "result.hpp"

#include <Wt/AsioWrapper/asio.hpp>

#include <postgresql/libpq-fe.h>
#include <stdexcept>
#include <iostream>
#include <list>

namespace postgrespp {

class result;
static constexpr int maxBatchCount = 256;

template <class DerivedT>
class socket_operations {
public:
  using result_t = result;

private:
  using error_code_t = boost::system::error_code;

protected:
  using derived_t = DerivedT;

protected:
  socket_operations() = default;

  socket_operations(const socket_operations&) = delete;
  socket_operations(socket_operations&&) = default;

  socket_operations& operator=(const socket_operations&) = delete;
  socket_operations& operator=(socket_operations&&) = default;

  ~socket_operations() = default;

  std::list<std::function<void()>> callable_; //store the SQL cmd : in non-pipeline mode you have to wait until the previous query return

  std::atomic<int> i_ = 0;

  template <class ResultCallableT>
  auto handle_exec(ResultCallableT&& handler) {
      auto initiation = [this](auto&& handler) { // lambda called via run or poll method from io_context


          boost::asio::post(derived().socket().get_executor(), [this, handler = std::move(handler)] () mutable {

              auto wrapped_handler = [handler = std::move(handler), r = std::make_shared<result>(nullptr)](auto&& res) mutable {
                  if (!res.done()) {

                      if(res.status() == result::status_t::PIPELINE_SYNC)
                          return;
                      //                  std::cout << "res status : " << (int)res.status() << std::endl;
                      //                  if(res.status() == result::status_t::TUPLES_OK)
                      //                      std::cout << "res size : " << res.size() << std::endl;

                      if (!r->done()) throw std::runtime_error{"expected one result"};
                      *r = std::move(res);
                  } else {
                      handler(std::move(*r));
                  }
              };

              on_write_ready({});
              wait_read_ready(std::move(wrapped_handler));

              i_.fetch_sub(1, std::memory_order_release);
              //std::cout << "i : " << i_ << std::endl;
              if(auto i = i_.load(std::memory_order_acquire); !i || i > maxBatchCount) { // [auto_batch] -> end batch           // cmd->sql_.length() > 1024
                  if (!PQpipelineSync(derived().connection().underlying_handle()))
                  {
                      std::cerr << "PQpipelineSync not working " << std::endl;
                  }
              }
          });


      };

      i_.fetch_add(1, std::memory_order_release);
      //std::cout << "i : " << i_ << std::endl;

      return boost::asio::async_initiate<ResultCallableT, void(result_t)>(initiation, handler);
  }

  template <class ResultCallableT>
  auto handle_exec2(ResultCallableT&& handler, auto&& func) {
      callable_.push_back(std::move(func));

      if(callable_.size() == 1) {
          (*callable_.begin())();
      }

      auto initiation = [this](auto&& handler) {
          auto wrapped_handler = [this, handler = std::move(handler), r = std::make_shared<result>(nullptr)](auto&& res) mutable {
              if (!res.done()) {
                  if (!r->done()) throw std::runtime_error{"expected one result"};
                  *r = std::move(res);
              } else {
                  handler(std::move(*r));

                  if(!callable_.empty()) {
                      callable_.pop_front();
                      if(callable_.size() > 0)
                          (*callable_.begin())();
                  }
              }
          };

          on_write_ready({});
          wait_read_ready(std::move(wrapped_handler));
      };

    return boost::asio::async_initiate<ResultCallableT, void(result_t)>(initiation, handler);
  }

  template <class ResultCallableT>
  auto handle_exec_all(ResultCallableT&& handler) {
    auto initiation = [this](auto&& handler) {
      on_write_ready({});
      wait_read_ready(std::move(handler));
    };

    return boost::asio::async_initiate<
      ResultCallableT, void(result_t)>(
          initiation, handler);
  }

private:
  template <class ResultCallableT>
  void wait_read_ready(ResultCallableT&& handler) {
    derived().socket().async_wait(std::decay_t<decltype(derived().socket())>::wait_read,
                                  [this, handler = std::move(handler)](auto&& ec) mutable {
                                      on_read_ready(std::move(handler), ec);
                                  });
  }

  void wait_write_ready() {
    namespace ph = std::placeholders;

    derived().socket().async_wait(std::decay_t<decltype(derived().socket())>::wait_write,
        std::bind(&socket_operations::on_write_ready, this, ph::_1));
  }

  template <class ResultCallableT>
  void on_read_ready(ResultCallableT&& handler, const error_code_t& ec) {
    const auto conn = derived().connection().underlying_handle();
    while (true) {
      if (PQconsumeInput(conn) != 1) {
        // TODO: convert this to some kind of error via the callback
        throw std::runtime_error{
          "consume input failed: " + std::string{derived().connection().last_error_message()}};
      }

      if (!PQisBusy(conn)) {
        const auto pqres = PQgetResult(conn);

//        auto type = PQresultStatus(pqres);
//        if (type == PGRES_BAD_RESPONSE || type == PGRES_FATAL_ERROR ||
//            type == PGRES_PIPELINE_ABORTED)
//        {
//            //handleFatalError(false);
//            std::cerr << " FATAL ERROR " << type << std::endl;
//            //break;
//        }
//        if (type == PGRES_PIPELINE_SYNC)
//        {
//            std::cerr << " PGRES_PIPELINE_SYNC " << std::endl;
////            if (batchCommandsForWaitingResults_.empty() &&
////                batchSqlCommands_.empty())
////            {
////                isWorking_ = false;
////                idleCb_();
////                return;
////            }
////            continue;
////            break;
//        }


        result res{pqres};
        handler(std::move(res));

        if (!pqres) {
            /*
             * No more results currtently available.
             */
//            if (!PQsendFlushRequest(conn)) //try query the last results still on server
//            {
//                //break;
//            }
//            auto res = PQgetResult(conn);
//            if (!res) //definitly no more result on this connection
//            {
//                break;
//            }
            break;
        }
      } else {
        wait_read_ready(std::forward<ResultCallableT>(handler));
        break;
      }
    }
  }

  void on_write_ready(const error_code_t& ec) {
    //PQpipelineSync(derived().connection().underlying_handle());
    const auto ret = PQflush(derived().connection().underlying_handle());

    //std::cout << "on_write_ready: " << ret << std::endl;
    if (ret == 1) {
      wait_write_ready();
    } else if (ret != 0) {
      // TODO: ignore or convert this to some kind of error via the callback
      throw std::runtime_error{"flush failed: " + std::string{derived().connection().last_error_message()}};
    }
  }

  derived_t& derived() { return *static_cast<derived_t*>(this); }
};

}
