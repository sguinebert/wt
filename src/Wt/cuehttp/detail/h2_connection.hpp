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

#include "connection.hpp"
#include "h2_mixin.hpp"

#include <openssl/ssl.h>

namespace Wt {
namespace http {
namespace detail {

template <typename _Socket = http_socket>
class h2_connection final : public base_connection<_Socket, h2_connection<_Socket>>,
                            public h2_mixin<h2_connection<_Socket>>,
                            safe_noncopyable {
public:
    template <typename... _Args>
    h2_connection(_Args&&... args) noexcept
        : base_connection<_Socket, h2_connection<_Socket>>{std::forward<_Args>(args)...} {}

    tcp::socket& socket() noexcept { return this->socket_; }

    void do_read_real() {
        co_spawn(this->socket_.get_executor(), this->coro_http(this->shared_from_this()), detached);
    }
};

#ifdef WT_WITH_SSL
template <>
class h2_connection<https_socket> final : public base_connection<https_socket, h2_connection<https_socket>>,
                                          public h2_mixin<h2_connection<https_socket>>,
                                          safe_noncopyable {
public:
    template <typename... _Args>
    h2_connection(_Args&&... args) noexcept : base_connection{std::forward<_Args>(args)...} {}

    tcp::socket& socket() noexcept { return socket_.next_layer(); }

    void do_read_real() {
        co_spawn(this->socket_.get_executor(), do_handshake(this->shared_from_this()), detached);
    }

private:
    awaitable<void> do_handshake(auto sft) {
        auto [ec] = co_await socket_.async_handshake(asio::ssl::stream_base::server, use_nothrow_awaitable);
        if (ec) {
            boost::system::error_code shutdown_ec;
            socket().shutdown(tcp::socket::shutdown_both, shutdown_ec);
            socket().close(shutdown_ec);
            co_return;
        }
        has_handshake_ = true;

        const unsigned char* alpn_data = nullptr;
        unsigned int alpn_len = 0;
        SSL_get0_alpn_selected(socket_.native_handle(), &alpn_data, &alpn_len);

        if (alpn_len == 2 && memcmp(alpn_data, "h2", 2) == 0) {
            this->setup_http2_session();
            co_spawn(socket_.get_executor(), this->h2_read_loop(sft), detached);
            co_spawn(socket_.get_executor(), this->h2_write_loop(sft), detached);
        } else {
            co_await this->coro_http(sft);
        }
    }

    bool has_handshake_{false};
};
#endif

}  // namespace detail
}  // namespace http
}  // namespace Wt
