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

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;

namespace Wt {
namespace http {
namespace detail {



// Returns something like "/tmp" or "C:\\Users\\Me\\AppData\\Local\\Temp"
static inline fs::path tempdir() {
    return fs::temp_directory_path();
}

// Creates a new zero-length file named wt-<random>.tmp in the temp dir.
// Returns its full path, or an empty path on error.
static inline fs::path tempfile() {
    try {
#ifdef _WIN32
        // wt‑1234.tmp  (4 hex digits + .tmp)
        fs::path name = fs::unique_path("wt-%%%%.tmp");
#else
        // wtXXXXXX    (6 hex digits, no extension)
        fs::path name = fs::unique_path("wt%%%%%%");
#endif
        return name;
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Temp file creation failed: " << e.what() << "\n";
        return {};
    }
}

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
    void track(nghttp2_rcbuf* key, nghttp2_rcbuf* val) {       // ➋ remember incref key values
        if(key)
            rcbufs_.emplace_back(key);
        if(val)
            rcbufs_.emplace_back(val);  
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
        body_channel_{io_context, channel_buffer_size}, gate_(io_context, 1),
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
    // stream(stream&& other) noexcept
    //     : session_(std::exchange(other.session_, nullptr)),
    //     body_channel_(std::move(other.body_channel_)),
    //     gate_(std::move(other.gate_)),
    //     headers(std::move(other.headers)),
    //     ioc_(other.ioc_),
    //     stream_id_(std::exchange(other.stream_id_, 0))
    // /* move other members */ {
    // }

    ~stream() {
    }

    auto make_nv(std::string_view n,
                 std::string_view v,
                 uint8_t flags = NGHTTP2_NV_FLAG_NONE) -> nghttp2_nv
    {
        return { reinterpret_cast<uint8_t*>(const_cast<char*>(n.data())),
                reinterpret_cast<uint8_t*>(const_cast<char*>(v.data())),
                n.size(),
                v.size(),
                flags };
    }

    asio::awaitable<void> handle_stream(const std::function<asio::awaitable<void>(context&)>& handler) {

        if (auto it = ctx.headers().find("content-type"); it != ctx.headers().end()) {
            /*Do not use multipart with modern HTTP2, we can handle multiple files at once with muliplexes streams: use application/octet-stream */
            if (/*it->second.find("multipart/form-data") != std::string::npos || */
                it->second.find("application/octet-stream") != std::string::npos) {
                is_upload = true;
            }
        }

        size_t total_size = 0;
        static auto maxpostLength = 10000000; // 10MB
        //auto postDataExceeded = ctx.postDataExceeded();
        //std::string temp_file_path;
        std::unique_ptr<asio::stream_file> temp_file;

        if (is_upload) {
            auto& reqHeaders = ctx.headers();
            if(auto upload = ctx.req().get("Upload"); !upload.empty()) {
                if(ctx.req().uploadedFiles().find(upload.data()) == ctx.req().uploadedFiles().end()) {



                    auto temp_file_name = tempfile();
                    fs::path temp_file_path   = tempdir() / temp_file_name;
                    if (temp_file_path.empty()) {
                        std::cerr << "Failed to create temporary file for upload.\n";
                        co_return;
                    }
                    auto& files = ctx.req().uploadedFiles();

                    files.emplace(
                        std::piecewise_construct,
                        std::forward_as_tuple(std::string(upload.data(), upload.size())), // key
                        std::forward_as_tuple(temp_file_name.string(),
                                              std::string(ctx.getHeader("filename")),
                                              std::string(ctx.getHeader("content-type")))
                        );
                    //ctx.req().uploadedFiles().emplace(upload.data(), temp_file_name.string(), ctx.getHeader("filename"), ctx.getHeader("content-type"));
                    // Create a temporary file for upload
                    temp_file = std::make_unique<asio::stream_file>(
                        body_channel_.get_executor(), temp_file_path.string(),
                        asio::stream_file::write_only | asio::stream_file::create | asio::stream_file::truncate);
                }
            }
        }



        for (;;) {
            auto [ec, chunk] = co_await body_channel_.async_receive(use_nothrow_awaitable);
            if (ec || chunk.empty()) break; // End of stream

            total_size += chunk.size();
            auto len = total_size - maxpostLength;
            // Check for maximum post size
            if (len > 0) {
                ctx.request_.postDataExceeded_ = len;

                co_await handler(ctx);

                std::vector<nghttp2_nv> error_headers = {
                    make_nv(":status", "413", NGHTTP2_NV_FLAG_NONE),
                    make_nv("content-type", "text/plain")
                };
                std::string error_message = "413 Payload Too Large";

                send_response(std::move(error_headers));
                end_response();
                co_return; //clean temp file?
            }

            if (temp_file) { //DO NOT USE multipart/form-data : need to be parsed first
                // (HTTP2 is multiplexed now) the multipart/form-data is less efficient
                co_await temp_file->async_write_some(asio::buffer(chunk), use_nothrow_awaitable);//catch errors...
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
    static ssize_t raw_read_cb(nghttp2_session* session, int32_t stream_id,
                         uint8_t* buf, size_t length, uint32_t* data_flags,
                         nghttp2_data_source* source, void*)
    {
        auto* stream = static_cast<class stream*>(source->ptr);
        std::string_view body_view = stream->ctx.res().body();
        size_t remaining = body_view.size() - stream->offset_;
        size_t to_send = std::min(length, remaining);
        if (to_send > 0) {
            /* copy into buf – you CANNOT just re-point the pointer */
            std::memcpy(buf, body_view.data() + stream->offset_, to_send);
            stream->offset_ += to_send;
        }
        if (stream->offset_ == body_view.size()) {
            if(stream->continuation_) {
                stream->paused_ = true;
                stream->gate_.try_send(boost::system::error_code());               // wake coroutine immediately
                return NGHTTP2_ERR_DEFERRED;
            }
            *data_flags |= NGHTTP2_DATA_FLAG_EOF;
        }
        return static_cast<ssize_t>(to_send);
    }
    static ssize_t brotli_read_cb(nghttp2_session*,
                                  int32_t /*stream_id*/,
                                  uint8_t* dst, size_t dst_len,
                                  uint32_t* data_flags,
                                  nghttp2_data_source* src,
                                  void* /*user*/)
    {
        auto* s     = static_cast<stream*>(src->ptr);

        /* 1. one encoder per stream ------------------------- */
        if (!s->br_) {
            s->br_ = BrotliEncoderCreateInstance(nullptr,nullptr,nullptr);
            if (!s->br_) return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
            BrotliEncoderSetParameter(s->br_, BROTLI_PARAM_QUALITY, 5);     // speed-quality trade-off
            BrotliEncoderSetParameter(s->br_, BROTLI_PARAM_MODE, BROTLI_MODE_GENERIC); //vs BROTLI_MODE_TEXT
        }

        /* 2. prepare input / output pointers ---------------- */
        std::string_view body = s->ctx.res().body();
        const uint8_t*   in   = reinterpret_cast<const uint8_t*>(body.data() + s->in_off_);
        size_t           avail_in  = body.size() - s->in_off_;
        uint8_t*         out       = dst;
        size_t           avail_out = dst_len;

        /* 3. encode one shot -------------------------------- */
        BrotliEncoderOperation op =
            (s->in_off_ + avail_in == body.size()) ? BROTLI_OPERATION_FINISH
                                                   : BROTLI_OPERATION_PROCESS;

        if (!BrotliEncoderCompressStream(s->br_, op,
                                         &avail_in, &in,
                                         &avail_out, &out, nullptr))
            return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;

        /* 4. update accounting ------------------------------ */
        size_t produced = dst_len - avail_out;
        s->in_off_ += (body.size() - s->in_off_) - avail_in;

        if (BrotliEncoderIsFinished(s->br_)) {
            // “continuation” logic (pause after slice)
            if(s->continuation_) {
                s->paused_ = true;
                s->gate_.try_send(boost::system::error_code());               // wake coroutine immediately
                return NGHTTP2_ERR_DEFERRED;
            }
            *data_flags |= NGHTTP2_DATA_FLAG_EOF;
            BrotliEncoderDestroyInstance(s->br_);
            s->br_      = nullptr;
            s->gz_done_ = true;
            return (ssize_t)produced;

        }
        else if (produced == 0) {
            /* dst too small – ask nghttp2 to call us again with fresh buffer */
            return NGHTTP2_ERR_WOULDBLOCK;
        }

        return static_cast<ssize_t>(produced);
    }


    static ssize_t gzip_read_cb(nghttp2_session* session, int32_t stream_id,
                        uint8_t* buf, size_t length, uint32_t* data_flags,
                        nghttp2_data_source* source, void*)
    {
        auto* s = static_cast<stream*>(source->ptr);

        constexpr int level = 6; // 1-9, 6 is default;
        if (!s->gz_ready_) {
            s->gz_ = z_stream{};
            constexpr int windowBits = 15 + 16;            // 15 = max window, +16 = gzip wrapper
            if (deflateInit2(&s->gz_, level, Z_DEFLATED, windowBits, level, Z_DEFAULT_STRATEGY) != Z_OK)
                return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;   // fatal for this stream
            s->gz_ready_ = true;
        }
        std::string_view body = s->ctx.res().body();

        s->gz_.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(body.data() + s->in_off_));
        s->gz_.avail_in = static_cast<uInt>(body.size() - s->in_off_);
        s->gz_.next_out = buf;                         // buf comes from nghttp2
        s->gz_.avail_out = static_cast<uInt>(length);   // how much we may fill

        int flush = (s->in_off_ + s->gz_.avail_in == body.size())
                        ? Z_FINISH : Z_NO_FLUSH;

        int ret   = deflate(&s->gz_, flush);
        if (ret < 0) return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;

        size_t produced = length - s->gz_.avail_out;
        s->in_off_     += (body.size() - s->in_off_) - s->gz_.avail_in;  // how much input consumed
        if (ret == Z_STREAM_END) {                   // all input consumed & gzip trailer written
            // “continuation” logic (pause after slice)
            if(s->continuation_) {
                s->paused_ = true;
                s->gate_.try_send(boost::system::error_code());               // wake coroutine immediately
                return NGHTTP2_ERR_DEFERRED;
            }
            *data_flags |= NGHTTP2_DATA_FLAG_EOF;
            deflateEnd(&s->gz_);
            s->gz_ready_ = false;
            s->gz_done_  = true;
            return (ssize_t)produced;
        }

        // if we produced 0 bytes because 'buf' was too small, ask nghttp2 to call us again
        if (produced == 0 && s->gz_.avail_out == 0)
            return NGHTTP2_ERR_WOULDBLOCK;           // unlikely with 16 KiB, but correct

        return (ssize_t)produced;
    }
/* Memory & flow-control
    ┌───────────── prepare slice 0
│ flush_chunk()
│            ┌── nghttp2 pulls slice 0 (read_cb) – gate send
│            │
│            │         ┌── kernel writes slice 0
│            │         │
│ prepare slice 1 ─────┘ resume coroutine (gate recv)
│ flush_chunk() again
└─────────────────────────────────────────
*/
    void send_response(std::vector<nghttp2_nv> headers) {
        if (!ctx.res().body().empty()) {// Send response using nghttp2 with zero-copy
            nghttp2_data_provider data_prd{};
            data_prd.source.ptr = this; // Use stream* directly
            data_prd.read_callback = deflate_ ? &gzip_read_cb : &raw_read_cb;
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
    void flush(bool deflate) {
        deflate_ = deflate;
        auto resHeaders = ctx.headers(); //correct this
        std::vector<nghttp2_nv> h2_resp;
        h2_resp.reserve(resHeaders.size() + 1);


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
        nghttp2_session_resume_data(session_, stream_id);
    }

    void end_response() {
        response_complete = true;
        offset_ = 0;
        //auto* conn = static_cast<base_connection*>(ctx.user_data);
        nghttp2_session_resume_data(session_, stream_id);
    }
    auto body_channel() noexcept -> stream_type& { return body_channel_; }


private:
    nghttp2_session* session_;
    int32_t stream_id;
    std::vector<nghttp2_nv> headers;
    asio::experimental::channel<void(boost::system::error_code, std::string)> body_channel_;
    asio::experimental::channel<void(boost::system::error_code)> gate_;
    bool response_complete = false;
    context ctx;
    bool is_upload = false;
    std::size_t offset_ = 0;
    bool paused_ = false;
    bool continuation_ = false;

    //gzip
    bool deflate_ = false;
    z_stream gz_{0};
    BrotliEncoderState* br_ = nullptr; //brotli
    bool gz_ready_ = false;
    bool gz_done_ = false;
    std::size_t in_off_ = 0;     // how much of body we have fed to zlib

    friend class http::response;
};


}  // namespace detail
}  // namespace http
}  // namespace cue

/* messy def position */
inline awaitable<void> Wt::http::response::http2_flush(bool deflate) {
    stream_->continuation_ = true;
    if(stream_->paused_)
        nghttp2_session_resume_data(stream_->session_, stream_->stream_id);
    else
        stream_->flush(deflate);

    co_await stream_->gate_.async_receive(use_awaitable);
}
