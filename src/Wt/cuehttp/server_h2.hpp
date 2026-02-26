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

#pragma once

#include "server.hpp"
#include "detail/h2_connection.hpp"

namespace Wt {
namespace http {

#ifdef WT_WITH_SSL
using https_h2_t = server<detail::https_socket, detail::h2_connection>;

struct https_h2 final : safe_noncopyable {
    template <typename... _Args>
    static https_h2_t create_server(_Args&&... args) noexcept {
        return https_h2_t{std::forward<_Args>(args)...};
    }
};
#endif

}  // namespace http
}  // namespace Wt
