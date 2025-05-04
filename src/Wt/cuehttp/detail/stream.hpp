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

#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <iostream>

#include "../context.hpp"
#include "endian.hpp"
#include "noncopyable.hpp"

#include <Wt/AsioWrapper/asio.hpp>
#include <nghttp2/nghttp2.h>

namespace Wt {
namespace http {
namespace detail {
struct rcbuf_handle {
    nghttp2_rcbuf* p = nullptr;

    rcbuf_handle() = default;
    explicit rcbuf_handle(nghttp2_rcbuf* q) : p(q) {}

    rcbuf_handle(const rcbuf_handle& o) : p(o.p) {
        if (p) nghttp2_rcbuf_incref(p);
    }
    rcbuf_handle& operator=(const rcbuf_handle& o) {
        if (this != &o) {
            if (o.p) nghttp2_rcbuf_incref(o.p);
            if (p)   nghttp2_rcbuf_decref(p);
            p = o.p;
        }
        return *this;
    }
    rcbuf_handle(rcbuf_handle&& o) noexcept : p(o.p) { o.p = nullptr; }
    rcbuf_handle& operator=(rcbuf_handle&& o) noexcept {
        if (p) nghttp2_rcbuf_decref(p);
        p = o.p; o.p = nullptr;
        return *this;
    }
    ~rcbuf_handle() {
        if (p) nghttp2_rcbuf_decref(p);
    }
};
struct nghttp2_headers {
public:
    nghttp2_headers() = default;
    void track(nghttp2_rcbuf* p, nghttp2_rcbuf* val) {       // ➋ remember one incref
        if(p) {
            rcbufs_.emplace_back(p);
        }
        if(val) {
            rcbufs_.emplace_back(val);
        }
    }
private:
    std::vector<rcbuf_handle> rcbufs_; // ➌ just raw pointers
};
class stream final : public noncopyable 
{
public:
    using context = Wt::http::context;
    using stream_type = asio::experimental::channel<void(boost::system::error_code, std::string)>;

    explicit stream(nghttp2_session* session, const nghttp2_frame* frame,
                    asio::io_context& io_context, size_t channel_buffer_size,
                    std::function<asio::awaitable<bool>(std::string_view)> reply_chunk,
                    std::function<asio::awaitable<bool>(std::vector<asio::const_buffer>&)> reply_chunk_sg,
                    bool is_ssl, std::function<void(detail::ws_frame&&)> ws_send)
        : session_{session},
        stream_id{frame->hd.stream_id},
        headers{frame->headers.nvlen},
        body_channel_{io_context, channel_buffer_size},
        ctx{std::move(reply_chunk), std::move(reply_chunk_sg), is_ssl, std::move(ws_send)}
    {
        // Extract headers from the frame
        if (frame && frame->hd.type == NGHTTP2_HEADERS) {
            for (size_t i = 0; i < frame->headers.nvlen; ++i) {

                if (frame->headers.nva[i].name && frame->headers.nva[i].value) {
                    nghttp2_nv header = frame->headers.nva[i];
                    std::string_view name(reinterpret_cast<const char*>(header.name), header.namelen);
                    std::string_view value(reinterpret_cast<const char*>(header.value), header.valuelen);
                    // Process HTTP/2 pseudo-headers
                    if (name == ":method") {
                        ctx.request_.method_ = value;
                    } else if (name == ":path") {
                        ctx.request_.url_ = value;
                        // Parse URL components using ada
                        auto uri = ada::parse<ada::url_aggregator>(value);
                        if (uri) {
                            ctx.request_.urlsv_ = uri.value();
                            ctx.request_.path_ = ctx.request_.urlsv_.get_pathname();
                            ctx.request_.querystring_ = ctx.request_.urlsv_.get_search();
                            ctx.request_.search_ = ctx.request_.querystring_;
                        }
                    } else if (name == ":scheme") {
                        ctx.request_.https_ = (value == "https");
                    } else if (name == ":authority") {
                        // Store as Host header for compatibility
                        ctx.request_.headers_.emplace("Host", value);
                    } else if (name == "content-length") {
                        try {
                            ctx.request_.content_length_ = std::stoull(std::string(value));
                        } catch (...) {
                            ctx.request_.content_length_ = 0;
                        }
                    } else if (name == "cookie") {
                        ctx.request_.cookies_.parse(value);
                    }
                    // } else if (header.namelen == 4 && memcmp(header.name, "user-agent", 10) == 0) {
                    //   ctx.user_agent = std::string(reinterpret_cast<const char*>(header.value), header.valuelen);
                    // } else if (header.namelen == 4 && memcmp(header.name, "accept", 6) == 0) {
                    //   ctx.accept = std::string(reinterpret_cast<const char*>(header.value), header.valuelen);
                    // } else if (header.namelen == 4 && memcmp(header.name, "accept-encoding", 15) == 0) {
                    //   ctx.accept_encoding = std::string(reinterpret_cast<const char*>(header.value), header.valuelen);
                    // } else if (header.namelen == 4 && memcmp(header.name, "accept-language", 16) == 0) {
                    //   ctx.accept_language = std::string(reinterpret_cast<const char*>(header.value), header.valuelen);
                    // } else if (header.namelen == 4 && memcmp(header.name, "accept-charset", 15) == 0) {
                    //   ctx.accept_charset = std::string(reinterpret_cast<const char*>(header.value), header.valuelen);
                    // } else if (header.namelen == 4 && memcmp(header.name, "cookie", 6) == 0) {
                    //   ctx.cookies = std::string(reinterpret_cast<const char*>(header.value), header.valuelen);
                    // } else if (header.namelen == 4 && memcmp(header.name, "referer", 7) == 0) {
                    //   ctx.referer = std::string(reinterpret_cast<const char*>(header.value), header.valuelen);
                    // } else if (header.namelen == 4 && memcmp(header.name, "origin", 6) == 0) {
                    //   ctx.origin = std::string(reinterpret_cast<const char*>(header.value), header.valuelen);
                    // } else if (header.namelen == 4 && memcmp(header.name, "upgrade", 7) == 0) {
                    //   ctx.upgrade = std::string(reinterpret_cast<const char*>(header.value), header.valuelen);
                    // } else if (header.namelen == 4 && memcmp(header.name, "connection", 10) == 0) {
                    //   ctx.connection = std::string(reinterpret_cast<const char*>(header.value), header.valuelen);
                    // } else if (header.namelen == 4 && memcmp(header.name, "pragma", 6) == 0) {
                    //   ctx.pragma = std::string(reinterpret_cast<const char*>(header.value), header.valuelen);
                    // } else if (header.namelen == 4 && memcmp(header.name, "cache-control", 13) == 0) {
                    //   ctx.cache_control = std::string(reinterpret_cast<const char*>(header.value), header.valuelen);
                    // }
                    //  ctx.if_none_match = std::string(reinterpret_cast<const char*>(header.value), header.valuelen);
                    // Store other headers
                    ctx.request_.headers_.emplace(name, value);
                }
            }
        }
    }

    ~stream() {


    }

    asio::awaitable<void> handle_stream(std::function<asio::awaitable<void>(context&)> handler) {

        if (auto it = ctx.headers().find("content-type"); it != ctx.headers().end()) {
            /*Do not use multipart with modern HTTP2, we can handle multiple files at once with muliplexes streams: use application/octet-stream */
            if (/*it->second.find("multipart/form-data") != std::string::npos || */
                it->second.find("application/octet-stream") != std::string::npos) {
                is_upload = true;
            }
        }

        size_t total_size = 0;
        //auto postDataExceeded = ctx.postDataExceeded();
        //std::string temp_file_path;
        //std::unique_ptr<asio::stream_file> temp_file;

        if (is_upload) {
            // Always use a temporary file for uploads
            // temp_file_path = (boost::filesystem::temp_directory_path() / 
            //                   boost::filesystem::unique_path()).string();
            // temp_file = std::make_unique<asio::stream_file>(
            //     body_channel.get_executor(), temp_file_path,
            //     asio::stream_file::write_only | asio::stream_file::create | 
            //     asio::stream_file::truncate);
        } 

        for (;;) {
            auto [ec, chunk] = co_await body_channel_.async_receive(use_nothrow_awaitable);
            if (ec || chunk.empty()) break; // End of stream

            total_size += chunk.size();
            // if (postDataExceeded) {
            //     std::vector<nghttp2_nv> error_headers = {
            //         { (uint8_t*)":status", 7, (uint8_t*)"413", 3, NGHTTP2_NV_FLAG_NONE }
            //     };
            //     send_response(std::move(error_headers), "Payload Too Large");
            //     end_response();
            //     break;
            // }

            if (is_upload) { //multipart/form-data need to be parsed first
                // Parse the multipart/form-data and write to temp_file
                //co_await temp_file->async_write_some(asio::buffer(chunk), use_nothrow_awaitable);
            } else {
                ctx.request_.buffer_.assign(chunk.begin(), chunk.end());
            }
        }

        ctx.request_.body_ = std::string_view(ctx.request_.buffer_.data(), ctx.request_.buffer_.size());
        // if (is_upload) {
        //     ctx.temp_file_path = temp_file_path;
        //     temp_file.reset();
        // }

        co_await handler(ctx);

        auto resHeaders = ctx.headers(); //correct this
        std::vector<nghttp2_nv> h2_resp;
        h2_resp.reserve(resHeaders.size() + 1);
        auto make_nv = [](std::string_view n,
                          std::string_view v,
                          uint8_t flags = NGHTTP2_NV_FLAG_NONE) -> nghttp2_nv {
            return { reinterpret_cast<uint8_t*>(const_cast<char*>(n.data())),
                    reinterpret_cast<uint8_t*>(const_cast<char*>(v.data())),
                    n.size(),
                    v.size(),
                    flags };
        };

        h2_resp.push_back(make_nv(":status", std::to_string(ctx.status())));      // 1️⃣ first & mandatory

        // 2) copy application headers, skipping hop‑by‑hop & pseudo
        static constexpr std::array<std::string_view, 7> hop_by_hop = {"connection",
                                                                       "keep-alive",
                                                                       "proxy-connection",
                                                                       "transfer-encoding",
                                                                       "upgrade",
                                                                       "trailer",
                                                                       "te"};

        for (const auto& [name, value] : resHeaders) {
            if (!name.empty() && name[0] != ':'                              // skip any stray pseudo
                && std::none_of(hop_by_hop.begin(), hop_by_hop.end(),
                                [&](auto h){ return h == name; }))           // skip banned headers
            {
                // translate Host → :authority
                if (name == "host") {
                    h2_resp.push_back(make_nv(":authority", value));
                } else {
                    h2_resp.push_back(make_nv(name, value));
                }
            }
        }

        send_response(std::move(h2_resp));
        end_response();
    }

    void send_response(std::vector<nghttp2_nv> headers) {
        if (!ctx.res().body().empty()) {// Send response using nghttp2 with zero-copy
            nghttp2_data_provider data_prd{};
            data_prd.source.ptr = this; // Use stream* directly
            data_prd.read_callback = [](nghttp2_session*, int32_t, uint8_t* buf, size_t length,
                                        uint32_t* data_flags, nghttp2_data_source* source,
                                        void*) -> ssize_t {
                auto* stream = static_cast<class stream*>(source->ptr);
                std::string_view body_view = stream->ctx.res().body();
                size_t remaining = body_view.size() - stream->offset;
                size_t to_send = std::min(length, remaining);
                if (to_send > 0) {
                    // Point buf directly to the original data without copying
                    buf = reinterpret_cast<uint8_t*>(const_cast<char*>(body_view.data() + stream->offset));
                    stream->offset += to_send;
                }
                if (stream->offset == body_view.size()) {
                    *data_flags |= NGHTTP2_DATA_FLAG_EOF;
                }
                return static_cast<ssize_t>(to_send);
            };
            int rv = nghttp2_submit_response(session_, stream_id, headers.data(),
                                             headers.size(), &data_prd);
            if (rv != 0) {
                throw std::runtime_error("Failed to submit response: " +
                                         std::string(nghttp2_strerror(rv)));
            }
        } else { // send response without body
            int rv = nghttp2_submit_response(session_, stream_id, headers.data(),
                                             headers.size(), nullptr);
            if (rv != 0) {
                throw std::runtime_error("Failed to submit response: " + 
                                         std::string(nghttp2_strerror(rv)));
            }
        }
    }

    void end_response() {
        response_complete = true;
        //auto* conn = static_cast<base_connection*>(ctx.user_data);
        nghttp2_session_resume_data(session_, stream_id);
    }
    auto body_channel() noexcept -> stream_type& { return body_channel_; }


private:
    nghttp2_session* session_;
    int32_t stream_id;
    std::vector<nghttp2_nv> headers;
    asio::experimental::channel<void(boost::system::error_code, std::string)> body_channel_;
    bool response_complete = false;
    context ctx;
    bool is_upload = false;
    std::size_t offset = 0;
};

}  // namespace detail
}  // namespace http
}  // namespace cue

