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

#ifndef CUEHTTP_DETAIL_STATIC_HPP_
#define CUEHTTP_DETAIL_STATIC_HPP_

#include "../context.hpp"
#include "../send.hpp"

namespace Wt {
namespace http {

namespace static_file {

struct options final {
  bool hidden{false};
  bool delay{false};
  std::string index;
  std::vector<std::string> extensions;
  bool cross_domain{false};
  std::map<std::string, std::string> mime_types;
#ifdef ENABLE_GZIP
  bool gzip{true};
  std::uint64_t gzip_threshold{2048};
#endif  // ENABLE_GZIP
};

}  // namespace static_file

namespace detail {

template <typename _Root, typename _Options>
inline auto use_static(_Root&& root, _Options&& options) noexcept {
  return [root = std::forward<_Root>(root), options = std::forward<_Options>(options)](context& ctx,
                                                                                       std::function<awaitable<void>()> next) -> awaitable<void> {
      static const auto handler = [](context& ctx, std::string&& root, const static_file::options& static_options) -> awaitable<void> {
          if (ctx.method() != "GET" && ctx.method() != "HEAD") {
              co_return;
          }
          send::options send_options;
          send_options.root = std::move(root);
          send_options.index = static_options.index.empty() ? "index.html" : std::move(static_options.index);
          send_options.extensions = std::move(static_options.extensions);
          send_options.hidden = static_options.hidden;
          send_options.cross_domain = static_options.cross_domain;
#ifdef ENABLE_GZIP
          send_options.gzip = static_options.gzip;
          send_options.gzip_threshold = static_options.gzip_threshold;
#endif  // ENABLE_GZIP
          co_await send_file(ctx, std::string{ctx.path()}, std::move(send_options));
          co_return;
      };

      if (options.delay) {
          co_await next();
          if (ctx.status() != 404 || ctx.has_body()) {
              co_return;
          }
          co_await handler(ctx, std::move(root), options);
      } else {
          co_await handler(ctx, std::move(root), options);
          co_await next();
      }
      co_return;
  };
}

template <typename _Root>
inline auto use_static(_Root&& root) noexcept {
  return use_static(std::forward<_Root>(root), static_file::options{});
}

}  // namespace detail
}  // namespace http
}  // namespace cue

#endif  // CUEHTTP_DETAIL_STATIC_HPP_
