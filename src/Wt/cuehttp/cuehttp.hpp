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

#ifndef CUEHTTP_CUEHTTP_HPP_
#define CUEHTTP_CUEHTTP_HPP_

#include <memory>

#include "detail/engines.hpp"
#include "detail/middlewares.hpp"
#include "detail/noncopyable.hpp"
#include "server.hpp"
#include "ws_server.hpp"

namespace Wt {
namespace http {

class cuehttp final : safe_noncopyable {
 public:
    cuehttp(detail::engines& engine) noexcept : engine_(engine)
    {
        http_handler_ = middlewares_.callback();
        ws_handler_ = [](context& ctx) -> awaitable<void> { ctx.status(503); co_return; };
    }

  void run() { engine_.run(); }

  void stop() { engine_.stop(); }

  cuehttp& listen(unsigned port) {
    assert(port != 0);
    server_ = http::create_server(callback(), &engine_);
    server_.listen(port);
    return *this;
  }

  template <typename _Host>
  cuehttp& listen(unsigned port, _Host&& host) {
    assert(port != 0);
    server_ = http::create_server(callback(), &engine_);
    server_.listen(port, std::forward<_Host>(host));
    return *this;
  }

  std::function<awaitable<void>(context&)> callback() const noexcept {
    return [this](context& ctx) -> awaitable<void> {
      if (ctx.req().websocket()) {
        co_await ws_handler_(ctx);
      } else {
        co_await http_handler_(ctx);
      }
      co_return;
    };
  }

  template <typename... _Args>
  cuehttp& use(_Args&&... args) {
    middlewares_.use(std::forward<_Args>(args)...);
    return *this;
  }

  ws_server& ws() {
    if (!ws_) {
      ws_ = std::make_unique<ws_server>();
      ws_handler_ = ws_->callback();
    }
    return *ws_;
  }

 private:
  detail::engines& engine_;
  http_t server_;
  detail::middlewares middlewares_;
  std::function<awaitable<void>(context&)> http_handler_;
  // websocket
  std::unique_ptr<ws_server> ws_;
  std::function<awaitable<void>(context&)> ws_handler_;
};

}  // namespace http
}  // namespace cue

#endif  // CUEHTTP_CUEHTTP_HPP_
