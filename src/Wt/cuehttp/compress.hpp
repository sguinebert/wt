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

#ifndef CUEHTTP_COMPRESS_HPP_
#define CUEHTTP_COMPRESS_HPP_

//#ifdef WTHTTP_WITH_ZLIB

#include "context.hpp"
#include "detail/gzip.hpp"

#include <Wt/AsioWrapper/asio.hpp>

namespace Wt {
namespace http {

struct compress final {
  struct options final {
    std::uint64_t threshold{2048};
    int level{8};
  };

  static bool deflate(std::string_view src, std::string& dst, int level = 8) {
    return detail::gzip::compress(src, dst, level);
  }
};

template <typename _Options>
inline auto use_compress(_Options&& options) noexcept {
  return [options = std::forward<_Options>(options)](context& ctx, std::function<awaitable<void>()> next) -> awaitable<void> {
    // call next ---> need to get the data first via the user defined function
    co_await next();

    if (ctx.req().method() == "HEAD") {
      co_return;
    }

    if (ctx.res().length() < options.threshold) {
      co_return;
    }

    const auto body = ctx.res().dump_body();
    std::string dst_body;
    if (!detail::gzip::compress(body, dst_body, options.level)) {
      ctx.status(500);
      co_return;
    }
    
    ctx.addHeader("Content-Encoding", "gzip");
    ctx.body(std::move(dst_body));
  };
}

inline auto use_compress() noexcept { return use_compress(compress::options{}); }

}  // namespace http
}  // namespace cue

//#endif  // WTHTTP_WITH_ZLIB

#endif  // CUEHTTP_COMPRESS_HPP_
