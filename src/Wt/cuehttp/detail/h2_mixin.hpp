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

#include <unordered_map>

#include <nghttp2/nghttp2.h>

#include "stream.hpp"

namespace Wt {
namespace http {
namespace detail {

template <typename Derived>
class h2_mixin {
public:
    ~h2_mixin() {
        if (session_) {
            nghttp2_session_del(session_);
        }
    }

protected:
    Derived& self() noexcept { return static_cast<Derived&>(*this); }

    void setup_http2_session() {
        nghttp2_session_callbacks* callbacks;
        nghttp2_session_callbacks_new(&callbacks);
        nghttp2_session_callbacks_set_on_header_callback2(callbacks, on_header_callback2);
        nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, on_frame_recv_callback);
        nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, on_data_chunk_recv_callback);
        nghttp2_session_callbacks_set_on_stream_close_callback(callbacks, on_stream_close_callback);
        nghttp2_session_callbacks_set_send_callback(callbacks, http2_frame_send_cb);
        nghttp2_session_server_new(&session_, callbacks, static_cast<void*>(&self()));
        nghttp2_session_callbacks_del(callbacks);

        // Submit server SETTINGS frame (required for HTTP/2 handshake)
        nghttp2_settings_entry iv[] = {
            {NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100}
        };
        nghttp2_submit_settings(session_, NGHTTP2_FLAG_NONE, iv, 1);
    }

    awaitable<void> h2_read_loop(auto sft) {
        unused(sft);
        std::vector<uint8_t> buffer(4096);
        for (;;) {
            auto [ec, bytes_read] = co_await self().socket_.async_read_some(asio::buffer(buffer), use_nothrow_awaitable);
            if (ec) {
                co_await self().close();
                co_return;
            }
            ssize_t rv = nghttp2_session_mem_recv(session_, buffer.data(), bytes_read);
            if (rv < 0) {
                co_await self().close();
                co_return;
            }
        }
    }

    awaitable<void> h2_write_loop(auto sft) {
        unused(sft);
        for (;;) {
            const uint8_t* data;
            ssize_t len = nghttp2_session_mem_send(session_, &data);
            if (len < 0) {
                co_await self().close();
                co_return;
            }
            if (len == 0 && !nghttp2_session_want_write(session_)) {
                co_await self().timer_.async_wait(use_nothrow_awaitable);
                continue;
            }
            auto [ec, bytes_written] = co_await asio::async_write(self().socket_, asio::buffer(data, len), use_nothrow_awaitable);
            if (ec) {
                co_await self().close();
                co_return;
            }
        }
    }

    awaitable<void> push_resource(int32_t stream_id, const std::string& path,
                                  const std::string& content_type, std::string_view content, int priority_weight = 16)
    {
        constexpr auto MAKE_NV = [](std::string_view n,
                                    std::string_view v,
                                    uint8_t flags = NGHTTP2_NV_FLAG_NONE) -> nghttp2_nv {
            return { reinterpret_cast<uint8_t*>(const_cast<char*>(n.data())),
                    reinterpret_cast<uint8_t*>(const_cast<char*>(v.data())),
                    n.size(),
                    v.size(),
                    flags };
        };
        nghttp2_nv push_headers[] = {
            MAKE_NV(":method", "GET"),
            MAKE_NV(":path", path.c_str()),
            MAKE_NV(":scheme", "https"),
            MAKE_NV(":authority", self().context_.req().host().data()),
            MAKE_NV("content-type", content_type.c_str())
        };

        int32_t promised_stream_id = nghttp2_submit_push_promise(
            session_, NGHTTP2_FLAG_NONE, stream_id,
            push_headers, sizeof(push_headers) / sizeof(push_headers[0]), nullptr);

        if (promised_stream_id < 0) {
            co_return;
        }

        nghttp2_priority_spec pri_spec;
        nghttp2_priority_spec_init(&pri_spec, stream_id, priority_weight, 0);
        nghttp2_submit_priority(session_, NGHTTP2_FLAG_NONE, promised_stream_id, &pri_spec);

        nghttp2_data_provider data_prov;
        data_prov.source.ptr = new std::string(content);
        data_prov.read_callback = [](nghttp2_session*, int32_t,
                                     uint8_t* buf, size_t length, uint32_t* data_flags,
                                     nghttp2_data_source* source, void*) -> ssize_t {
            auto* data = static_cast<std::string*>(source->ptr);
            if (data->empty()) {
                *data_flags |= NGHTTP2_DATA_FLAG_EOF;
                return 0;
            }
            size_t to_copy = std::min(length, data->size());
            std::memcpy(buf, data->data(), to_copy);
            *data = data->substr(to_copy);
            if (data->empty()) {
                *data_flags |= NGHTTP2_DATA_FLAG_EOF;
            }
            return to_copy;
        };

        nghttp2_submit_response(session_, promised_stream_id, push_headers,
                                sizeof(push_headers) / sizeof(push_headers[0]), &data_prov);
        co_return;
    }

private:
    static ssize_t data_provider_read_callback(nghttp2_session*, int32_t stream_id, uint8_t* buf,
                                               size_t length, uint32_t* data_flags,
                                               nghttp2_data_source* source, void* user_data) {
        auto* conn = static_cast<Derived*>(user_data);
        auto it = conn->streams_.find(stream_id);
        if (it == conn->streams_.end()) return 0;
        auto& stream = it->second;
        if (!stream->response_queue.empty()) {
            auto& chunk = stream->response_queue.front();
            size_t to_copy = std::min(length, chunk.size());
            std::memcpy(buf, chunk.data(), to_copy);
            if (to_copy < chunk.size()) {
                chunk = chunk.substr(to_copy);
            } else {
                stream->response_queue.pop();
            }
            return static_cast<ssize_t>(to_copy);
        } else if (stream->response_complete) {
            *data_flags |= NGHTTP2_DATA_FLAG_EOF;
            return 0;
        } else {
            return NGHTTP2_ERR_DEFERRED;
        }
    }

    static int on_header_callback2(nghttp2_session*, const nghttp2_frame* frame,
                                   nghttp2_rcbuf* name, nghttp2_rcbuf* value,
                                   uint8_t, void* user_data)
    {
        if (frame->headers.cat != NGHTTP2_HCAT_REQUEST) {
            return 0;
        }
        auto* conn = static_cast<Derived*>(user_data);
        auto it = conn->ng_headers_.find(frame->hd.stream_id);
        if (it == conn->ng_headers_.end()) {
            auto& headers = conn->ng_headers_[frame->hd.stream_id];
            headers.track(name, value);
        } else {
            it->second.track(name, value);
        }
        return 0;
    }

    static int on_frame_recv_callback(nghttp2_session* session, const nghttp2_frame* frame, void* user_data)
    {
        auto* conn = static_cast<Derived*>(user_data);
        if (frame->hd.type == NGHTTP2_HEADERS && frame->headers.cat == NGHTTP2_HCAT_REQUEST)
        {
            // Retrieve headers accumulated by on_header_callback2
            // (frame->headers.nva is always NULL when on_header_callback2 is set)
            auto hit = conn->ng_headers_.find(frame->hd.stream_id);
            const auto& nvs = (hit != conn->ng_headers_.end()) ? hit->second.nvs() : empty_nvs_;

            auto& ioc = static_cast<asio::io_context&>(conn->socket_.get_executor().context());
            auto [s, ok] =
                conn->streams_.try_emplace(frame->hd.stream_id,
                                           session, frame->hd.stream_id, nvs,
                                           ioc, 10,
                                           [conn](std::string_view sv) { return conn->coro_reply_chunk(sv); },
                                           [conn](std::vector<asio::const_buffer>& bufs) { return conn->coro_reply_chunk_sg(bufs); },
                                           false,
                                           [conn](detail::ws_frame&& f) { conn->spawn_coro_ws_send(std::move(f)); });
            asio::co_spawn(ioc,
                           s->second.handle_stream(conn->handler_),
                           detached);
        }
        if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
            if (auto it = conn->streams_.find(frame->hd.stream_id); it != conn->streams_.end()) {
                it->second.body_channel().close();
            }
        }
        return 0;
    }

    static int on_data_chunk_recv_callback(nghttp2_session*, uint8_t, int32_t stream_id,
                                           const uint8_t* data, size_t len, void* user_data)
    {
        auto* conn = static_cast<Derived*>(user_data);
        if (auto it = conn->streams_.find(stream_id); it != conn->streams_.end()) {
            boost::system::error_code ec;
            it->second.body_channel().async_send(ec, std::string(reinterpret_cast<const char*>(data), len), detached);
        }
        return 0;
    }

    static int on_stream_close_callback(nghttp2_session*, int32_t stream_id,
                                        uint32_t, void* user_data)
    {
        auto* conn = static_cast<Derived*>(user_data);
        conn->streams_.erase(stream_id);
        conn->ng_headers_.erase(stream_id);
        return 0;
    }

    static ssize_t http2_frame_send_cb(nghttp2_session*,
                                       const uint8_t*,
                                       size_t length,
                                       int,
                                       void* user_data)
    {
        auto* conn = static_cast<Derived*>(user_data);
        conn->timer_.cancel();
        return length;
    }

    static inline const std::vector<nghttp2_nv> empty_nvs_{};

protected:
    nghttp2_session* session_{nullptr};
    std::unordered_map<int32_t, stream> streams_;
    std::unordered_map<int32_t, nghttp2_headers> ng_headers_;
};

}  // namespace detail
}  // namespace http
}  // namespace Wt
