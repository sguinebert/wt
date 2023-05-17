/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef CUEHTTP_ENGINES_HPP_
#define CUEHTTP_ENGINES_HPP_

#include <memory>
#include <vector>

#include <boost/asio.hpp>
#include "../detail/noncopyable.hpp"

#ifdef TWEAKS
#define BOOST_ASIO_DISABLE_THREADS 1
#endif

#ifdef TWEAKS
asio::io_context ctx{BOOST_ASIO_CONCURRENCY_HINT_UNSAFE};
#else
//asio::io_context ctx{1};
#endif

using namespace boost;
using executor_t = boost::asio::io_context::executor_type;
inline static thread_local asio::io_context* thread_context = nullptr;

namespace Wt {
namespace http {
namespace detail {

class engines final : safe_noncopyable {
 public:
  explicit engines(std::size_t size) noexcept {
    assert(size != 0);
    for (std::size_t i{0}; i < size; ++i) {
      auto io_context = std::make_shared<asio::io_context>(1);
      asio::post(*io_context, [ctx = io_context.get()] { thread_context = ctx; });

      auto worker = std::make_shared<asio::io_context::work>(*io_context);
      io_contexts_.emplace_back(std::move(io_context));
      workers_.emplace_back(std::move(worker));
    }
  }

  static engines& default_engines() noexcept {
    static engines engines{std::thread::hardware_concurrency()};
    return engines;
  }

  static asio::io_context* get_thread_context() {
    return thread_context;
  }

  asio::io_context& get() noexcept { return *io_contexts_[index_++ % io_contexts_.size()]; }

  void run() {
    for (const auto& io_context : io_contexts_) {
      run_threads_.emplace_back([io_context]() { thread_context = io_context.get(); io_context->run(); });
    }

    for (auto& run_thread : run_threads_) {
      if (run_thread.joinable()) {
        run_thread.join();
      }
    }
  }

  void stop() {
    workers_.clear();
    for (const auto& io_context : io_contexts_) {
      io_context->stop();
    }
  }

  template <class Token>
  auto schedule(std::chrono::steady_clock::duration millis, Token&& handler)
  {
    auto initiator = [millis] (auto&& handler) {
        if (millis.count() == 0)
            asio::post(*thread_context, [cb = std::move(handler)] () { cb(); }); // guarantees execution order
        else {
            std::shared_ptr<asio::steady_timer> timer = std::make_shared<asio::steady_timer>(*thread_context);
            timer->expires_from_now(millis);
            timer->async_wait([timer, cb = std::move(handler)] (auto /*ec*/) { cb(); });
        }
    };
    return asio::async_initiate<Token, void()>(initiator, handler);
  }

  template <class Token>
  void dispatchAll(Token&& handler)
  {
    for(unsigned i(0); i < io_contexts_.size(); i++)
      asio::dispatch(*io_contexts_[i], handler);
  }

 private:
  std::vector<std::shared_ptr<asio::io_context>> io_contexts_;
  std::vector<std::shared_ptr<asio::io_context::work>> workers_;
  std::vector<std::thread> run_threads_;
  std::size_t index_{0};
};

}  // namespace detail
}  // namespace http
}  // namespace cue

#endif  // CUEHTTP_ENGINES_HPP_
