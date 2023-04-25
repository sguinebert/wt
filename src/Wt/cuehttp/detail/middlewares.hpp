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

#ifndef CUEHTTP_MIDDLEWARES_HPP_
#define CUEHTTP_MIDDLEWARES_HPP_

#include <functional>
#include <vector>

#include "../context.hpp"
#include "common.hpp"
#include "noncopyable.hpp"

namespace cue {
namespace http {
namespace detail {

class middlewares final : safe_noncopyable {
 public:
  middlewares() noexcept { compose(); }

  //const std::function<void(context&)>& callback() const noexcept { return handler_; }
  const std::function<asio::awaitable<void>(context&)>& callback() const noexcept { return handler_; }

  template <typename... _Args>
  void use(_Args&&... args) {
    use_impl(std::forward<_Args>(args)...);
  }

 private:
  template <typename _List, typename = std::enable_if_t<detail::is_middleware_list_v<_List>>>
  void use_impl(_List&& list) {
    use_append_list(std::forward<_List>(list));
  }

//  void use_append_list(const std::vector<std::function<void(context&, std::function<void()>)>>& handlers) {
//    middlewares_.insert(middlewares_.end(), handlers.begin(), handlers.end());
//  }

//  void use_append_list(std::vector<std::function<void(context&, std::function<void()>)>>&& handlers) {
//    middlewares_.insert(middlewares_.end(), std::make_move_iterator(handlers.begin()),
//                        std::make_move_iterator(handlers.end()));
//  }

  void use_append_list(const std::vector<std::function<asio::awaitable<void>(context&, std::function<asio::awaitable<void>()>)>>& handlers) {
    middlewares_.insert(middlewares_.end(), handlers.begin(), handlers.end());
  }

  void use_append_list(std::vector<std::function<asio::awaitable<void>(context&, std::function<asio::awaitable<void>()>)>>&& handlers) {
    middlewares_.insert(middlewares_.end(), std::make_move_iterator(handlers.begin()),
                        std::make_move_iterator(handlers.end()));
  }

  template <typename _Func, typename = std::enable_if_t<!std::is_member_function_pointer_v<_Func>>>
  std::enable_if_t<detail::is_middleware_v<_Func>, std::true_type>
  use_impl(_Func&& func) {
    use_with_next(std::forward<_Func>(func));
    return std::true_type{};
  }

  template <typename _Ty, typename _Func, typename _Self, typename = std::enable_if_t<std::is_same_v<_Ty*, _Self>>>
  void use_impl(_Func (_Ty::*func)(context&, std::function<asio::awaitable<void>()>), _Self self) {
    use_with_next(func, self);
  }

  template <typename _Ty, typename _Func>
  void use_impl(_Func (_Ty::*func)(context&, std::function<asio::awaitable<void>()>)) noexcept {
    use_with_next(func, (_Ty*)nullptr);
  }

  template <typename _Func, typename = std::enable_if_t<!std::is_member_function_pointer_v<_Func>>>
  std::enable_if_t<detail::is_middleware_without_next_v<_Func>, std::false_type> use_impl(_Func&& func) {
    use_without_next(std::forward<_Func>(func));
    return std::false_type{};
  }

  template <typename _Ty, typename _Func, typename _Self, typename = std::enable_if_t<std::is_same_v<_Ty*, _Self>>>
  void use_impl(_Func (_Ty::*func)(context&), _Self self) {
    use_without_next(func, self);
  }

  template <typename _Ty, typename _Func>
  void use_impl(_Func (_Ty::*func)(context&)) {
    use_without_next(func, (_Ty*)nullptr);
  }

  template <typename _Func>
  void use_with_next(_Func&& func) {
    middlewares_.emplace_back(std::forward<_Func>(func));
  }

  template <typename _Ty, typename _Func, typename _Self>
  void use_with_next(_Func _Ty::*func, _Self self) {
    middlewares_.emplace_back([func, self](context& ctx, std::function<asio::awaitable<void>()> next) -> asio::awaitable<void> {
      if (self) {
        (self->*func)(ctx, std::move(next));
      } else {
        (_Ty{}.*func)(ctx, std::move(next));
      }
      co_return;
    });
  }

  template <typename _Func>
  void use_without_next(_Func&& func) {
    middlewares_.emplace_back( //= static_cast<std::function<void(context&)>>(func)
        [func = static_cast<std::function<asio::awaitable<void>(context&)>>(func)](context& ctx, std::function<asio::awaitable<void>()> next) -> asio::awaitable<void> {
        //if constexpr (std::is_same<>)
          co_await func(ctx);
          co_await next();
        });
  }

  template <typename _Ty, typename _Func, typename _Self>
  void use_without_next(_Func _Ty::*func, _Self self) {
    middlewares_.emplace_back([func, self](context& ctx, std::function<void()> next) -> asio::awaitable<void> {
      if (self) {
        (self->*func)(ctx);
      } else {
        (_Ty{}.*func)(ctx);
      }
      next();
      co_return;
    });
  }

  void compose() noexcept {
    handler_ = [this](context& ctx) -> asio::awaitable<void> {
      if (middlewares_.empty()) {
        co_return;
      }

      if (middlewares_.size() == 1) {
        co_await middlewares_[0](ctx, []() -> asio::awaitable<void> { co_return; });
      } else {
        std::size_t index{0};
        std::function<asio::awaitable<void>()> next;
        next = [this, &next, &index, &ctx]() -> asio::awaitable<void> {
          if (++index == middlewares_.size()) {
            co_return;
          }
          co_await middlewares_[index](ctx, next);
        };

        co_await middlewares_[0](ctx, next);
      }
    };
  }

//  std::function<void(context&)> handler_;
//  std::vector<std::function<void(context&, std::function<void()>)>> middlewares_;

  std::function<asio::awaitable<void>(context&)> handler_;
  std::vector<std::function<asio::awaitable<void>(context&, std::function<asio::awaitable<void>()>)>> middlewares_;
};

}  // namespace detail
}  // namespace http
}  // namespace cue

#endif  // CUEHTTP_MIDDLEWARES_HPP_
