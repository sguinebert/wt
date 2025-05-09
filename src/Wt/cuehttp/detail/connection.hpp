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

#ifndef CUEHTTP_CONNECTION_HPP_
#define CUEHTTP_CONNECTION_HPP_

#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>
#include <iostream>

#include <Wt/AsioWrapper/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <nghttp2/nghttp2.h>
#include <boost/system/error_code.hpp>
#include <openssl/ssl.h>

// #if defined(__has_include) && __has_include(<boost/unordered/unordered_flat_map.hpp>)
// #include <boost/unordered/unordered_flat_map.hpp>
// #define WT_HAS_BOOST_UNORDERED_FLAT_MAP 1
// #else
// #include <unordered_map>
// #endif

#include "stream.hpp"
#include "../context.hpp"
#include "endian.hpp"
#include "noncopyable.hpp"

namespace Wt {
namespace http {
namespace detail {

using asio::ip::tcp;

template <typename _Socket, typename _Ty>
class base_connection : public std::enable_shared_from_this<base_connection<_Socket, _Ty>>, safe_noncopyable {
public:
    template <typename Socket = _Socket, typename = std::enable_if_t<std::is_same_v<std::decay_t<Socket>, http_socket>>>
    base_connection(std::function<awaitable<void>(context&)> handler, asio::io_context& io_service) noexcept
        : socket_{io_service}, timer_{io_service, asio::steady_timer::duration::max()},
        context_{std::bind(&base_connection::coro_reply_chunk, this, std::placeholders::_1),
                 std::bind(&base_connection::coro_reply_chunk_sg, this, std::placeholders::_1),
                 false,
                 std::bind(&base_connection::spawn_coro_ws_send, this, std::placeholders::_1)},
        handler_{std::move(handler)},
        session_{nullptr} {}

#ifdef WT_WITH_SSL
    template <typename Socket = _Socket, typename = std::enable_if_t<!std::is_same_v<std::decay_t<Socket>, http_socket>>>
    base_connection(std::function<awaitable<void>(context&)> handler, asio::io_context& io_service,
                    asio::ssl::context& ssl_context) noexcept
        : socket_{io_service, ssl_context}, timer_{io_service, asio::steady_timer::duration::max()},
        context_{std::bind(&base_connection::coro_reply_chunk, this, std::placeholders::_1),
                 std::bind(&base_connection::coro_reply_chunk_sg, this, std::placeholders::_1),
                 true,
                 std::bind(&base_connection::spawn_coro_ws_send, this, std::placeholders::_1)},
        handler_{std::move(handler)},
        session_{nullptr} {
        context_.setSSLcontext(ssl_context.native_handle());
    }
#endif

    virtual ~base_connection() {
        if (session_) {
            nghttp2_session_del(session_);
        }
    }

    tcp::socket& socket() noexcept { return static_cast<_Ty&>(*this).socket(); }

    void run() { do_read(); }

protected:
    static ssize_t data_provider_read_callback(nghttp2_session* session, int32_t stream_id, uint8_t* buf,
                                               size_t length, uint32_t* data_flags,
                                               nghttp2_data_source* source, void* user_data) {
        auto* conn = static_cast<base_connection*>(user_data);
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

    static int on_header_callback2(nghttp2_session *session, const nghttp2_frame *frame,
                                   nghttp2_rcbuf *name, nghttp2_rcbuf *value, 
                                   uint8_t flags, void *user_data) 
    {
      // Only for request headers
      if (frame->headers.cat != NGHTTP2_HCAT_REQUEST) {
        return 0;
      }
      auto* conn = static_cast<base_connection*>(user_data);
      auto it = conn->ng_headers_.find(frame->hd.stream_id);
      if(it == conn->ng_headers_.end()) {
          auto& headers = conn->ng_headers_[frame->hd.stream_id];
          headers.track(name, value);
      }
      else
        it->second.track(name, value);

      return 0;
    }

    static int on_frame_recv_callback(nghttp2_session* session, const nghttp2_frame* frame, void* user_data)
    {
        auto* conn = static_cast<base_connection*>(user_data);
        if (frame->hd.type == NGHTTP2_HEADERS && frame->headers.cat == NGHTTP2_HCAT_REQUEST)
        {
            auto& ioc = static_cast<asio::io_context&>(conn->socket_.get_executor().context());
            auto [s, ok] =
                conn->streams_.try_emplace(frame->hd.stream_id,
                                           session, frame,
                                           ioc, 10,
                                           std::bind(&base_connection::coro_reply_chunk, conn, std::placeholders::_1),
                                           std::bind(&base_connection::coro_reply_chunk_sg, conn, std::placeholders::_1),
                                           false,
                                           std::bind(&base_connection::spawn_coro_ws_send, conn, std::placeholders::_1));
            //conn->streams_[frame->hd.stream_id] = std::move(s);
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

    static int on_data_chunk_recv_callback(nghttp2_session* session, uint8_t flags, int32_t stream_id,
                                           const uint8_t* data, size_t len, void* user_data)
    {
        auto* conn = static_cast<base_connection*>(user_data);
        if (auto it = conn->streams_.find(stream_id); it != conn->streams_.end()) {
            //it->second->body_channel().try_send({}, std::string(reinterpret_cast<const char*>(data), len));
            boost::system::error_code ec;
            it->second.body_channel().async_send(ec, std::string(reinterpret_cast<const char*>(data), len), detached);
        }
        return 0;
    }

    static int on_stream_close_callback(nghttp2_session* session, int32_t stream_id,
                                        uint32_t error_code, void* user_data)
    {
        auto* conn = static_cast<base_connection*>(user_data);
        conn->streams_.erase(stream_id);
        conn->ng_headers_.erase(stream_id);
        return 0;
    }
    static ssize_t http2_frame_send_cb(nghttp2_session* session,
                                       const uint8_t* data,
                                       size_t length,
                                       int flags,
                                       void* user_data)
    {
        auto* conn = static_cast<base_connection*>(user_data);
        conn->timer_.cancel();
        return length;
    }

    void setup_http2_session() {
        nghttp2_session_callbacks* callbacks;
        nghttp2_session_callbacks_new(&callbacks);
        nghttp2_session_callbacks_set_on_header_callback2(callbacks, on_header_callback2);
        nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, on_frame_recv_callback);
        nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, on_data_chunk_recv_callback);
        nghttp2_session_callbacks_set_on_stream_close_callback(callbacks, on_stream_close_callback);
        nghttp2_session_callbacks_set_send_callback(callbacks, http2_frame_send_cb);
        nghttp2_session_server_new(&session_, callbacks, this);
        nghttp2_session_callbacks_del(callbacks);
    }

    awaitable<void> read_loop(auto sft) {
        detail::unused(sft);
        std::vector<uint8_t> buffer(4096);
        for (;;) {
            auto [ec, bytes_read] = co_await socket_.async_read_some(asio::buffer(buffer), use_nothrow_awaitable);
            if (ec) {
                co_await close();
                co_return;
            }
            ssize_t rv = nghttp2_session_mem_recv(session_, buffer.data(), bytes_read);
            if (rv < 0) {
                co_await close();
                co_return;
            }
        }
    }

    awaitable<void> write_loop(auto sft) {
        detail::unused(sft);
        for (;;) {
            const uint8_t* data;
            ssize_t len = nghttp2_session_mem_send(session_, &data);
            if (len < 0) {
                co_await close();
                co_return;
            }
            if (len == 0 && !nghttp2_session_want_write(session_)) {
                co_await timer_.async_wait(use_nothrow_awaitable);
                continue;
            }
            auto [ec, bytes_written] = co_await asio::async_write(socket_, asio::buffer(data, len), use_nothrow_awaitable);
            if (ec) {
                co_await close();
                co_return;
            }
        }
    }

    // awaitable<void> write_loop() {
    //     for (;;) {
    //         const uint8_t* data;
    //         ssize_t len;
    //         while ((len = nghttp2_session_mem_send(session_, &data)) > 0) {
    //             auto [ec, bytes_written] = co_await asio::async_write(socket_, asio::buffer(data, len), use_nothrow_awaitable);
    //             if (ec) {
    //                 co_await close();
    //                 co_return;
    //             }
    //         }
    //         if (len < 0) {
    //             co_await close();
    //             co_return;
    //         }
    //         co_await timer_.async_wait(use_nothrow_awaitable);
    //     }
    // }

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
        // Create headers for the pushed resource
        nghttp2_nv push_headers[] = {
            MAKE_NV(":method", "GET"),
            MAKE_NV(":path", path.c_str()),
            MAKE_NV(":scheme", "https"),  // or "http" depending on connection
            MAKE_NV(":authority", context_.req().host().data()),
            MAKE_NV("content-type", content_type.c_str())
        };

        // Submit the push promise
        int32_t promised_stream_id = nghttp2_submit_push_promise(
            session_, NGHTTP2_FLAG_NONE, stream_id,
            push_headers, sizeof(push_headers) / sizeof(push_headers[0]), nullptr);

        if (promised_stream_id < 0) {
            co_return; // Failed to create push promise
        }

        // Set priority for the pushed stream
        nghttp2_priority_spec pri_spec;
        nghttp2_priority_spec_init(&pri_spec, stream_id, priority_weight, 0);
        nghttp2_submit_priority(session_, NGHTTP2_FLAG_NONE, promised_stream_id, &pri_spec);

        // Create a data provider for the pushed content
        nghttp2_data_provider data_prov;
        data_prov.source.ptr = new std::string(content);  // Will need to be freed later
        data_prov.read_callback = [](nghttp2_session* session, int32_t stream_id,
                                     uint8_t* buf, size_t length, uint32_t* data_flags,
                                     nghttp2_data_source* source, void* user_data) -> ssize_t {
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

        // Submit the pushed response
        nghttp2_submit_response(session_, promised_stream_id, push_headers,
                                sizeof(push_headers) / sizeof(push_headers[0]), &data_prov);
        co_return;
    }

    awaitable<void> close() {
        if (ws_handshake_) {
            co_await ws_helper_->websocket_->emit(detail::ws_event::close);
            ws_handshake_ = false;
            cancel_signal_.emit(asio::cancellation_type::total);
        }
        timer_.cancel();
        boost::system::error_code ec;
        socket().shutdown(tcp::socket::shutdown_both, ec);
        socket().close(ec);
        co_return;
    }

    void do_read() { static_cast<_Ty&>(*this).do_read_real(); }

    awaitable<void> coro_http(auto sft) {
        for (;;) {
            auto buffer = context_.req().buffer();
            auto [ec, bytes] = co_await socket_.async_read_some(
                asio::buffer(buffer.first, buffer.second), use_nothrow_awaitable);
            if (ec) {
                if (ec == asio::error::eof) {
                    boost::system::error_code shutdown_ec;
                    socket().shutdown(tcp::socket::shutdown_both, shutdown_ec);
                }
                co_return;
            }
            auto finished = true;
            switch (context_.req().parse(bytes)) {
            case 0:
                co_await handle();
                break;
            case -1:
                reply_str_.append(make_reply_str(400));
                break;
            case -3:
                co_await handle();
                for (;;) {
                    auto code = context_.req().parse(0);
                    if (code == 0) {
                        co_await handle();
                        break;
                    } else if (code == -1) {
                        reply_str_.append(make_reply_str(400));
                        break;
                    } else if (code == -2) {
                        co_await handle();
                        finished = false;
                        break;
                    } else {
                        co_await handle();
                    }
                }
                if (!finished) continue;
                break;
            case -2:
                finished = false;
                continue;
            default:
                continue;
            }
            if (!context_.flush_) {
                co_await context_.wait_flush(asio::use_awaitable);
            }
            if (!context_.static_reply_.empty()) {
                auto [ec, bytes] = co_await asio::async_write(socket_, asio::buffer(context_.static_reply_), use_nothrow_awaitable);
                if (ec) {
                    co_await close();
                    co_return;
                }
                context_.reset();
                continue;
            }
            std::vector<asio::const_buffer> buffers;
            context_.res().to_buffers(buffers);
            auto [_ec, _bytes] = co_await asio::async_write(socket_, buffers, use_nothrow_awaitable);
            if (_ec) {
                co_await close();
                co_return;
            }
            reply_str_.clear();
            if (ws_helper_) {
                ws_handshake_ = true;
                co_spawn(socket_.get_executor(), coro_ws(sft), detached);
                co_await ws_helper_->websocket_->emit(detail::ws_event::open);
                co_return;
            }
            if (finished) context_.reset();
        }
    }

    inline awaitable<void> handle() {
        auto& req = context_.req();
        auto& res = context_.res();
        res.minor_version(req.minor_version());
        if (req.websocket() && !ws_helper_) {
            ws_helper_ = std::make_unique<ws_helper>();
            ws_helper_->websocket_ = context_.websocket_ptr();
        }
        co_await handler_(context_);
        co_return;
    }

    awaitable<bool> coro_reply_chunk(std::string_view chunk) {
        auto [ec, bytes] = co_await asio::async_write(socket_, asio::buffer(chunk), use_nothrow_awaitable);
        co_return !!ec;
    }

    awaitable<bool> coro_reply_chunk_sg(std::vector<asio::const_buffer>& chunk) {
        auto [ec, bytes] = co_await asio::async_write(socket_, chunk, use_nothrow_awaitable);
        co_return !!ec;
    }

    void spawn_coro_ws_send(detail::ws_frame&& frame) {
        std::unique_lock<std::mutex> lock{ws_helper_->write_queue_mutex_};
        ws_helper_->write_queue_.emplace(std::move(frame));
        lock.unlock();
        cancel_signal_.emit(asio::cancellation_type::total);
    }

    awaitable<void> coro_ws(auto /*sft*/) {
        co_await (coro_do_read_ws_header() || coro_do_send_ws_frame());
    }

    awaitable<void> coro_do_read_ws_header() {
        for (;;) {
            auto [ec, bytes] = co_await asio::async_read(socket_, asio::buffer(ws_helper_->ws_reader_.header), use_nothrow_awaitable);
            if (ec) {
                co_await close();
                co_return;
            }
            auto& reader = ws_helper_->ws_reader_;
            reader.fin = reader.header[0] & 0x80;
            reader.opcode = static_cast<detail::ws_opcode>(reader.header[0] & 0xf);
            reader.zip = reader.header[0] & 0x40;
            reader.has_mask = reader.header[1] & 0x80;
            reader.length = reader.header[1] & 0x7f;
            if (reader.length == 126) {
                co_await coro_do_read_ws_length_and_mask(2);
            } else if (reader.length == 127) {
                co_await coro_do_read_ws_length_and_mask(8);
            } else {
                co_await coro_do_read_ws_length_and_mask(0);
            }
        }
    }

    awaitable<void> coro_do_read_ws_length_and_mask(std::size_t bytes) {
        auto& reader = ws_helper_->ws_reader_;
        const auto length = bytes + (reader.has_mask ? 4 : 0);
        if (length == 0) {
            co_await coro_handle_ws();
        } else {
            reader.length_mask_buffer.resize(length);
            auto [ec, bytes_transferred] = co_await asio::async_read(
                socket_, asio::buffer(reader.length_mask_buffer.data(), length), use_nothrow_awaitable);
            if (ec) {
                co_await close();
                co_return;
            }
            if (bytes == 2) {
                reader.length = detail::from_be(*reinterpret_cast<std::uint16_t*>(reader.length_mask_buffer.data()));
                if (reader.has_mask) {
                    memcpy(reader.mask, reader.length_mask_buffer.data() + 2, 4);
                }
            } else if (bytes == 8) {
                reader.length = detail::from_be(*reinterpret_cast<std::uint64_t*>(reader.length_mask_buffer.data()));
                if (reader.has_mask) {
                    memcpy(reader.mask, reader.length_mask_buffer.data() + 8, 4);
                }
            } else {
                if (reader.has_mask) {
                    memcpy(reader.mask, reader.length_mask_buffer.data(), 4);
                }
            }
            co_await coro_do_read_ws_payload();
        }
    }

    awaitable<void> coro_do_read_ws_payload() {
        auto& reader = ws_helper_->ws_reader_;
        const std::size_t length{reader.payload_buffer.size()};
        reader.payload_buffer.resize(reader.length + length);
        auto [ec, bytes] = co_await asio::async_read(
            socket_, asio::buffer(reader.payload_buffer.data() + length, reader.length), use_nothrow_awaitable);
        if (ec) {
            co_await close();
            co_return;
        }
        if (reader.has_mask) {
            for (std::size_t i{0}; i < reader.length; ++i) {
                reader.payload_buffer[i] ^= reader.mask[i % 4];
            }
        }
        co_await coro_handle_ws();
    }

    awaitable<void> coro_handle_ws() {
        auto& reader = ws_helper_->ws_reader_;
        if (reader.zip) {
            // Placeholder for inflate logic if needed
        }
        switch (reader.opcode) {
        case detail::ws_opcode::continuation:
            reader.last_fin = false;
            break;
        case detail::ws_opcode::text:
        case detail::ws_opcode::binary:
            if (reader.fin) {
                reader.last_fin = true;
                auto& payload = reader.payload_buffer;
                co_await ws_helper_->websocket_->emit(detail::ws_event::msg, {payload.data(), payload.size()});
                payload.clear();
            } else {
                reader.last_fin = false;
            }
            break;
        case detail::ws_opcode::close:
            co_await close();
            co_return;
        case detail::ws_opcode::ping:
            co_await coro_reply_ws_pong();
            break;
        default:
            break;
        }
    }

    awaitable<void> coro_reply_ws_pong() {
        detail::ws_frame frame;
        frame.opcode = detail::ws_opcode::pong;
        co_await coro_ws_send(std::move(frame));
        co_return;
    }

    awaitable<void> coro_ws_send(detail::ws_frame&& frame) {
        std::unique_lock<std::mutex> lock{ws_helper_->write_queue_mutex_};
        ws_helper_->write_queue_.emplace(std::move(frame));
        co_return;
    }

    awaitable<void> coro_do_send_ws_frame() {
        asio::steady_timer timer(socket_.get_executor(), asio::steady_timer::time_point::max());
        co_await timer.async_wait(asio::bind_cancellation_slot(cancel_signal_.slot(), use_nothrow_awaitable));
        for (;;) {
            if (!ws_handshake_) co_return;
            auto& frame = get_frame();
            std::ostream os{&ws_helper_->buffer_};
            auto opcode = static_cast<std::uint8_t>(frame.opcode) | 0x80;
            os.write(reinterpret_cast<char*>(&opcode), 1);
            std::uint8_t base_length{0};
            std::uint16_t length16{0};
            std::uint64_t length64{0};
            const auto size = frame.payload.size();
            if (size < 126) {
                base_length |= static_cast<std::uint8_t>(size);
                os.write(reinterpret_cast<char*>(&base_length), sizeof(std::uint8_t));
            } else if (size <= UINT16_MAX) {
                base_length |= 0x7e;
                os.write(reinterpret_cast<char*>(&base_length), sizeof(std::uint8_t));
                length16 = detail::to_be(static_cast<std::uint16_t>(size));
                os.write(reinterpret_cast<char*>(&length16), sizeof(std::uint16_t));
            } else {
                base_length |= 0x7f;
                os.write(reinterpret_cast<char*>(&base_length), sizeof(std::uint8_t));
                length64 = detail::to_be(static_cast<std::uint64_t>(size));
                os.write(reinterpret_cast<char*>(&length64), sizeof(std::uint64_t));
            }
            if (size > 0) {
                os.write(frame.payload.data(), size);
            }
            auto [ec, bytes] = co_await asio::async_write(socket_, ws_helper_->buffer_, use_nothrow_awaitable);
            if (ec) {
                co_await close();
                co_return;
            }
            std::unique_lock<std::mutex> lock{ws_helper_->write_queue_mutex_};
            ws_helper_->write_queue_.pop();
            if (ws_helper_->write_queue_.empty()) {
                lock.unlock();
                co_await timer.async_wait(asio::bind_cancellation_slot(cancel_signal_.slot(), use_nothrow_awaitable));
            }
        }
    }

    std::string make_reply_str(unsigned status) const {
        return std::string{detail::utils::get_response_line(1000 + status)};
    }

    struct ws_helper final {
        std::shared_ptr<websocket> websocket_;
        asio::streambuf buffer_;
        detail::ws_reader ws_reader_;
        std::queue<detail::ws_frame> write_queue_;
        std::mutex write_queue_mutex_; //contention should be low if no message sent from other clients (threads)
    };

    detail::ws_frame& get_frame() {
        std::unique_lock<std::mutex> lock{ws_helper_->write_queue_mutex_};
        assert(!ws_helper_->write_queue_.empty());
        return ws_helper_->write_queue_.front();
    }

    _Socket socket_;
    context context_;
    std::function<awaitable<void>(context&)> handler_;
    std::string reply_str_;
    bool ws_handshake_{false};
    std::unique_ptr<ws_helper> ws_helper_;
    asio::steady_timer timer_;
    asio::cancellation_signal cancel_signal_;
    nghttp2_session* session_;
#if WT_HAS_BOOST_UNORDERED_FLAT_MAP
    boost::unordered_flat_map<int32_t, stream> streams_;
    boost::unordered_flat_map<int32_t, nghttp2_headers> ng_headers_;
#else
    std::unordered_map<int32_t, stream> streams_;
    std::unordered_map<int32_t, nghttp2_headers> ng_headers_;
#endif
};

template <typename _Socket = http_socket>
class connection final : public base_connection<_Socket, connection<_Socket>>, safe_noncopyable {
public:
    template <typename... _Args>
    connection(_Args&&... args) noexcept : base_connection<_Socket, connection<_Socket>>{std::forward<_Args>(args)...} {}

    tcp::socket& socket() noexcept { return this->socket_; }

    void do_read_real() {
        co_spawn(this->socket_.get_executor(), this->coro_http(this->shared_from_this()), detached);
    }
};

#ifdef WT_WITH_SSL
template <>
class connection<https_socket> final : public base_connection<https_socket, connection<https_socket>>,
                                       safe_noncopyable {
public:
    template <typename... _Args>
    connection(_Args&&... args) noexcept : base_connection{std::forward<_Args>(args)...} {}

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
            setup_http2_session();
            co_spawn(socket_.get_executor(), read_loop(sft), detached);
            co_spawn(socket_.get_executor(), write_loop(sft), detached);
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

#endif  // CUEHTTP_CONNECTION_HPP_
