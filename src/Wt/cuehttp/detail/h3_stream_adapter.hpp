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

#include "../context.hpp"
#include "../nexus/h3/stream.hpp"
#include "../nexus/h3/fields.hpp"

namespace Wt {
namespace http {
namespace detail {

/// Bridges a nexus::h3::stream to a cuehttp context, so the same
/// std::function<awaitable<void>(context&)> handler serves HTTP/3.
///
/// Header-only — all heavy lifting is done by the nexus .cc compiled
/// units already in libwhttp.
inline awaitable<void> handle_h3_stream(
    nexus::h3::stream& stream,
    nexus::h3::fields& req_fields,
    const std::function<awaitable<void>(context&)>& handler)
{
    // --- 1. Build reply handlers that write to the h3 stream ---

    bool headers_sent = false;
    nexus::h3::fields resp_fields;

    // Helper: ensure response headers are sent before any body data
    auto send_response_headers = [&]() -> awaitable<void> {
        if (headers_sent) co_return;
        headers_sent = true;
        nexus::error_code ec;
        co_await stream.async_write_headers(resp_fields, asio::redirect_error(asio::use_awaitable, ec));
    };

    // reply_handler: writes a string_view chunk to the h3 stream
    auto reply_chunk = [&](std::string_view sv) -> awaitable<bool> {
        co_await send_response_headers();
        boost::system::error_code ec;
        co_await asio::async_write(stream, asio::buffer(sv.data(), sv.size()),
                                   asio::redirect_error(asio::use_awaitable, ec));
        co_return !ec;
    };

    // reply_handler_sg: scatter-gather write
    auto reply_chunk_sg = [&](std::vector<asio::const_buffer>& bufs) -> awaitable<bool> {
        co_await send_response_headers();
        for (auto& buf : bufs) {
            boost::system::error_code ec;
            co_await asio::async_write(stream, asio::buffer(buf),
                                       asio::redirect_error(asio::use_awaitable, ec));
            if (ec) co_return false;
        }
        co_return true;
    };

    // ws_send noop — no WebSocket over HTTP/3 (yet)
    auto ws_noop = [](ws_frame&&) {};

    // --- 2. Create context ---
    context ctx{std::move(reply_chunk), std::move(reply_chunk_sg),
                true /*https — QUIC is always TLS*/, std::move(ws_noop)};

    // --- 3. Populate request from h3::fields ---
    auto& req = ctx.req();
    for (const auto& field : req_fields) {
        auto name = field.name();
        auto value = field.value();

        if (name == ":method") {
            req.set_method(value);
        } else if (name == ":path") {
            req.set_h2_path(value);
        } else if (name == ":scheme") {
            req.set_https(value == "https");
        } else if (name == ":authority") {
            req.mutable_headers().emplace("Host", value);
        } else if (name == "content-length") {
            try {
                req.set_content_length(std::stoull(std::string(value)));
            } catch (...) {
                req.set_content_length(0);
            }
        } else if (name == "cookie") {
            req.parse_cookies(value);
        }

        // Store all headers (including pseudo-headers for completeness)
        req.mutable_headers().emplace(name, value);
    }

    // --- 4. Read request body ---
    {
        auto& buf = req.mutable_buffer();
        char tmp[4096];
        for (;;) {
            boost::system::error_code ec;
            auto n = co_await stream.async_read_some(
                asio::buffer(tmp), asio::redirect_error(asio::use_awaitable, ec));
            if (ec || n == 0) break;
            buf.insert(buf.end(), tmp, tmp + n);
        }
        req.set_body_from_buffer();
    }

    // --- 5. Set flush callback (reuses h2_flush_fn_t) ---
    ctx.res().set_h2_flush([&](bool /*deflate*/) -> awaitable<void> {
        // Ensure headers are sent on first flush
        co_await send_response_headers();
        // Body data is written through reply_handler by response::chunk_flush()
    });

    // --- 6. Invoke user handler ---
    co_await handler(ctx);

    // --- 7. Write response if not already streamed ---
    // Build response h3::fields from response headers
    static constexpr std::array<std::string_view, 7> hop_by_hop = {
        "connection", "keep-alive", "proxy-connection",
        "transfer-encoding", "upgrade", "trailer", "te"
    };

    resp_fields.insert(":status", std::to_string(ctx.status()));
    for (const auto& [name, value] : ctx.res().headers()) {
        if (name.empty() || name[0] == ':') continue;
        if (std::ranges::find(hop_by_hop, name) != hop_by_hop.end()) continue;
        resp_fields.insert(name, std::string(value));
    }

    if (!headers_sent) {
        // Normal (non-streaming) response: send headers + body
        co_await send_response_headers();

        auto body = ctx.res().body();
        if (!body.empty()) {
            boost::system::error_code ec;
            co_await asio::async_write(stream, asio::buffer(body.data(), body.size()),
                                       asio::redirect_error(asio::use_awaitable, ec));
        }
    }

    // --- 8. Shutdown write side ---
    {
        boost::system::error_code ec;
        stream.shutdown(1, ec); // shutdown write
    }
}

}  // namespace detail
}  // namespace http
}  // namespace Wt
