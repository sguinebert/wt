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
#include <iostream>

#include "../context.hpp"
#include "endian.hpp"
#include "noncopyable.hpp"

#include <Wt/AsioWrapper/asio.hpp>

/* https://stackoverflow.com/questions/17742741/websocket-data-unmasking-multi-byte-xor */
//  void xorWithMaskSIMD(std::vector<char>& payload_buffer, const std::array<char, 4>& mask) {
//    const size_t size = payload_buffer.size();
//    const size_t simdSize = size / 16; // Process 16 bytes at a time
//    const size_t remainingSize = size % 16;

//    // Cast mask to __m128i type
//    __m128i xmmMask = _mm_loadu_si128(reinterpret_cast<const __m128i*>(mask.data()));

//    // Process SIMD blocks
//    for (size_t i = 0; i < simdSize; ++i) {
//      // Load 16 bytes from payload_buffer
//      __m128i xmmPayload = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&payload_buffer[i * 16]));

//      // XOR with the mask
//      __m128i xmmResult = _mm_xor_si128(xmmPayload, xmmMask);

//      // Store the result back to payload_buffer
//      _mm_storeu_si128(reinterpret_cast<__m128i*>(&payload_buffer[i * 16]), xmmResult);
//    }

//    // Process remaining bytes
//    for (size_t i = simdSize - remainingSize; i < size; ++i) {
//      payload_buffer[i] ^= mask[i % 4];
//    }
//  }

//  void xorWithMaskAVX(std::vector<char>& payload_buffer, const std::array<char, 4>& mask) {
//    const size_t size = payload_buffer.size();
//    const size_t avxSize = size / 32; // Process 32 bytes at a time
//    const size_t remainingSize = size % 32;

//    // Repeat the 4-byte mask to match the payload length
//    //__m256i ymmMask = _mm256_set1_epi32(*reinterpret_cast<const int32_t*>(mask.data()));

//    // Cast mask to __m128i type
//    __m128i xmmMask = _mm_loadu_si128(reinterpret_cast<const __m128i*>(mask.data()));

//    // Process AVX blocks
//    for (size_t i = 0; i < avxSize; ++i) {
//      // Load 32 bytes from payload_buffer
//      __m256i ymmPayload = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&payload_buffer[i * 32]));

//      // XOR with the mask
//      __m256i ymmResult = _mm256_xor_si256(ymmPayload, _mm256_cvtepu8_epi32(xmmMask));

//      // Store the result back to payload_buffer
//      _mm256_storeu_si256(reinterpret_cast<__m256i*>(&payload_buffer[i * 32]), ymmResult);
//    }

//    // Process remaining bytes
//    for (size_t i = size - remainingSize; i < size; ++i) {
//      payload_buffer[i] ^= mask[i % 4];
//    }
//  }



namespace Wt {
namespace http {
namespace detail {

template <typename _Socket, typename _Ty>
class base_connection : public std::enable_shared_from_this<base_connection<_Socket, _Ty>>, safe_noncopyable {
 public:
  template <typename Socket = _Socket, typename = std::enable_if_t<std::is_same_v<std::decay_t<Socket>, http_socket>>>
  base_connection(std::function<awaitable<void>(context&)> handler, asio::io_service& io_service) noexcept
      : socket_{io_service},
        context_{std::bind(&base_connection::coro_reply_chunk, this, std::placeholders::_1), std::bind(&base_connection::reply_chunk, this, std::placeholders::_1), false,
                 std::bind(&base_connection::spawn_coro_ws_send, this, std::placeholders::_1)},
        handler_{std::move(handler)} {}

  virtual ~base_connection() = default;

#ifdef ENABLE_HTTPS
  template <typename Socket = _Socket, typename = std::enable_if_t<!std::is_same_v<std::decay_t<Socket>, http_socket>>>
  base_connection(std::function<void(context&)> handler, asio::io_service& io_service,
                  asio::ssl::context& ssl_context) noexcept
      : socket_{io_service, ssl_context},
        context_{std::bind(&base_connection::reply_chunk, this, std::placeholders::_1), true,
                 std::bind(&base_connection::send_ws_frame, this, std::placeholders::_1)},
        handler_{std::move(handler)} {}
#endif  // ENABLE_HTTPS

  asio::ip::tcp::socket& socket() noexcept { return static_cast<_Ty&>(*this).socket(); }

  void run() { do_read(); }

 protected:
  awaitable<void> close() {
    if (ws_handshake_) {
      co_await ws_helper_->websocket_->emit(detail::ws_event::close);
      ws_handshake_ = false;
    }
    boost::system::error_code code;
    socket().shutdown(asio::ip::tcp::socket::shutdown_both, code);
    socket().close(code);
    co_return;
  }

  void do_read() { static_cast<_Ty&>(*this).do_read_real(); }

//  awaitable<void> cancel_slot() {
//    std::cout << "trigger is running" << std::endl;
//    for(;;) {
//      asio::steady_timer timer(co_await asio::this_coro::executor, 1s);
//      co_await timer.async_wait(use_awaitable);
//      cancel_signal_.emit(asio::cancellation_type::total);
//      std::cout << "cancelled async operatiosns" << std::endl;
//    }
//  }
//  awaitable<void> Trigger(asio::steady_timer& timer) {
//    std::cout << "trigger is running" << std::endl;
//    //std::this_thread::sleep_for(std::chrono::seconds(1));
//    asio::steady_timer timer2(co_await asio::this_coro::executor, 1s);
//    co_await timer2.async_wait(use_awaitable);
//    //auto n = timer.cancel();
//    //std::cout << "cancelled " << n << " async operatiosns" << std::endl;
//    cancel_signal_.emit(asio::cancellation_type::total);
//    std::cout << "cancelled async operatiosns" << std::endl;
//    co_return;
//  }

  awaitable<void> coro_http(auto /*sft*/) {

//    std::cout << "test async_wait" << std::endl;
//    asio::steady_timer timer(co_await asio::this_coro::executor, asio::steady_timer::time_point::max());
//    co_await timer.async_wait(asio::bind_cancellation_slot(cancel_signal_.slot(), use_nothrow_awaitable));
//    std::cout << "second test async_wait" << std::endl;
//    co_await timer.async_wait(asio::bind_cancellation_slot(cancel_signal_.slot(), use_nothrow_awaitable));
//    std::cout << "test async_wait" << std::endl;

    for(;;) {
      auto buffer = context_.req().buffer();
      auto [code, bytes_transferred] = co_await socket_.async_read_some(asio::buffer(buffer.first, buffer.second), use_nothrow_awaitable);
      if (code) {
          //std::cerr << "error : async_read_some = " << code.what() << std::endl;
          if (code == asio::error::eof) {
              boost::system::error_code shutdown_code;
              socket().shutdown(asio::ip::tcp::socket::shutdown_both, shutdown_code);
          }
          co_return;
      }

      reply_str_.reserve(4096);
      //const auto finished = parse(bytes_transferred);
      auto finished = true;

      const auto parse_code = context_.req().parse(bytes_transferred);
      // = 0 success = -1 error = -2 not complete = -3 pipeline
      switch (parse_code) {
      case 0:
          co_await handle();
          finished = true;
          break;
      case -1:
          reply_str_.append(make_reply_str(400));
          finished = true;
          break;
      case -3:
          co_await handle();
          for (;;) {
              const auto code = context_.req().parse(0);
              if (code == 0) {
                  co_await handle();
                  finished = true;
              } else if (code == -1) {
                  reply_str_.append(make_reply_str(400));
                  finished = true;
              } else if (code == -2) {
                  co_await handle();
                  finished = false;
              } else {
                  co_await handle();
              }
          }
          break;
      case -2:
          finished = false;
          break;
      default:
          //        do_read();
          //std::cout <<" continue loop" << std::endl;
          continue;
          break;
      }

      if(!context_.flush_) {
          co_await context_.wait_flush(use_awaitable);
      }

      std::vector<asio::const_buffer> buffers;
      context_.res().to_buffers(buffers);

      { //reply(finished);
          auto [code2, bytes_transferred2] = co_await asio::async_write(socket_, buffers, use_nothrow_awaitable);
          detail::unused(bytes_transferred2);
          if (code2) {
              //continue;
              std::cerr << "error async_write: " << code2.what() << std::endl;
              co_return;
          }

          reply_str_.clear();
          if (ws_helper_) {
              ws_handshake_ = true;

              co_spawn(socket_.get_executor(), coro_ws(this->shared_from_this()), detached);
              co_await ws_helper_->websocket_->emit(detail::ws_event::open);
              //do_read_ws_header();
              co_return; //end this http coro
          } else {
              if (finished) {
                  context_.reset();
              }
              //do_read(); --> go to start
          }
      }
    }

    co_return;
  }

  inline awaitable<void> handle() {
    //assert(handler_);
    auto& req = context_.req();
    auto& res = context_.res();
    res.minor_version(req.minor_version());
    if (req.websocket() && !ws_helper_) {
      ws_helper_ = std::make_unique<ws_helper>();
      ws_helper_->websocket_ = context_.websocket_ptr();
    }
    co_await handler_(context_);
//    if (!res.is_stream()) {
//      res.to_string(reply_str_);
//    }
    co_return;
  }

  awaitable<void> flush(bool finished) {
    { //reply(finished);
      auto [code2, bytes_transferred2] = co_await asio::async_write(socket_, asio::buffer(reply_str_), use_nothrow_awaitable);
      detail::unused(bytes_transferred2);
      if (code2) {
          //continue;
          std::cerr << "error async_write: " << code2.what() << std::endl;
          co_return;
      }

      reply_str_.clear();
      if (ws_helper_) {
          ws_handshake_ = true;

          co_spawn(socket_.get_executor(), coro_ws(this->shared_from_this()), detached);
          co_await ws_helper_->websocket_->emit(detail::ws_event::open);
          //do_read_ws_header();
          co_return; //end this http coro
      } else {
          if (finished) {
              context_.reset();
          }
          //do_read(); --> go to start
      }
    }
  }

  awaitable<bool> coro_reply_chunk(std::string_view chunk) {
    auto [code, bytes_transferred] = co_await asio::async_write(socket_, asio::buffer(chunk), use_nothrow_awaitable);
    co_return !!code;
  }

  bool reply_chunk(std::string_view chunk) {
    boost::system::error_code code;
    asio::write(socket_, asio::buffer(chunk), code);
    return !!code;
  }

  awaitable<void> coro_ws(auto /*sft*/) {

    std::cerr << "start websocket coro_ws" << std::endl;
    //co_await coro_do_read_ws_header();
    co_await(coro_do_read_ws_header() || coro_do_send_ws_frame()); // todo watchdog


    std::cerr << "close coro_ws" << std::endl;
  }

  awaitable<void> coro_do_read_ws_header() {
    for(;;) {
      //do_read_ws_header()
      auto [code, bytes_transferred] = co_await asio::async_read(socket_, asio::buffer(ws_helper_->ws_reader_.header), use_nothrow_awaitable);
      detail::unused(bytes_transferred);
      if (code) {
          std::cerr << "close : " << code.what() << std::endl;
          co_await close();
          co_return;
      } else {
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
    co_return;
  }

  awaitable<void> coro_do_read_ws_length_and_mask(std::size_t bytes) {
    auto& reader = ws_helper_->ws_reader_;
    const auto length = bytes + (reader.has_mask ? 4 : 0);
    if (length == 0) {
      //handle_ws();
      co_await coro_handle_ws();
    } else {
      reader.length_mask_buffer.resize(length);
      auto [code, bytes_transferred] = co_await asio::async_read(socket_, asio::buffer(reader.length_mask_buffer.data(), length), use_nothrow_awaitable);
      detail::unused(bytes_transferred);
      if (code) {

          std::cerr << "close : " << code.what() << std::endl;
          co_await close();
          //if (code == asio::error::eof)
          co_return;
      } else {
          auto& reader = ws_helper_->ws_reader_;
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
          //do_read_ws_payload();
          co_await coro_do_read_ws_payload();
      }
    }
  }

  awaitable<void> coro_do_read_ws_payload() {
    auto& reader = ws_helper_->ws_reader_;
    const std::size_t length{reader.payload_buffer.size()};
    reader.payload_buffer.resize(reader.length + length);
    auto [code, bytes_transferred] = co_await asio::async_read(socket_, asio::buffer(reader.payload_buffer.data() + length, reader.length), use_nothrow_awaitable);
    detail::unused(bytes_transferred);
    if (code) {
        std::cerr << "close : " << code.what() << std::endl;
      co_await close();
      co_return;
    } else {
        auto& reader = ws_helper_->ws_reader_;
        auto& payload_buffer = reader.payload_buffer;
        if (reader.has_mask) {
            for (std::size_t i{0}; i < reader.length; ++i) {
                payload_buffer[i] ^= reader.mask[i % 4];
            }
        }
        //handle_ws();
        co_await coro_handle_ws();
    }
  }

  awaitable<void> coro_handle_ws() {
    auto& reader = ws_helper_->ws_reader_;

    if(reader.zip) {
        /* RSV1-3 must be 0 */
//        if (frameType & 0x70 && (!req.pmdState_.enabled && frameType & 0x30))
//            return Request::Error;
        //inflate
        //auto opcode = (detail::ws_opcode)(reader.opcode & 0x0F);
//        if (wsState_ < ws13_frame_start) {
//            if (wsFrameType_ == 0x00)
//                opcode = Reply::text_frame;
//        }
//        unsigned char appendBlock[] = { 0x00, 0x00, 0xff, 0xff };
//        bool hasMore = false;
//        char buffer[16 * 1024];
//        do {
//            uint64_t read_ = 0;
//            bool ret1 =  inflate(reinterpret_cast<unsigned char*>(&*beg),
//                                end - beg, reinterpret_cast<unsigned char*>(buffer), hasMore);

//            if (!ret1)
//                return Request::Error;

//            bool ret2 = reply->consumeWebSocketMessage(opcode, &buffer[0], &buffer[read_], hasMore ? Request::Partial : state);

//            if (!ret2)
//                return Request::Error;
//        } while (hasMore);

//        if (state == Request::Complete)
//            if(!inflate(appendBlock, 4, reinterpret_cast<unsigned char*>(buffer), hasMore))
//                return Request::Error;
    }

    switch (reader.opcode) {
    case detail::ws_opcode::continuation:
        reader.last_fin = false;
        break;
    case detail::ws_opcode::text:
    case detail::ws_opcode::binary: {
        if (reader.fin) {
            reader.last_fin = true;
            auto& payload_buffer = reader.payload_buffer;
            co_await ws_helper_->websocket_->emit(detail::ws_event::msg, {payload_buffer.data(), payload_buffer.size()});
            payload_buffer.clear();
        } else {
            reader.last_fin = false;
        }
        break;
    }
    case detail::ws_opcode::close:
        std::cerr << "ws_opcode close : " << std::endl;
        co_await close();
        co_return;
    case detail::ws_opcode::ping:
        co_await coro_reply_ws_pong();
        break;
    default:
        break;
    }
    //do_read_ws_header();
  }

  awaitable<void> coro_reply_ws_pong() {
    detail::ws_frame frame;
    frame.opcode = detail::ws_opcode::pong;
    co_await coro_ws_send(std::move(frame));
    co_return;
  }

  void spawn_coro_ws_send(detail::ws_frame&& frame) { //TODO : find a way to keep the coroutine running indefinitly
#ifndef WS_WSPAWN
    std::unique_lock<std::mutex> lock{ws_helper_->write_queue_mutex_};
    ws_helper_->write_queue_.emplace(std::move(frame));
    lock.unlock();
    cancel_signal_.emit(asio::cancellation_type::total);
#else
    co_spawn(this->socket_.get_executor(), coro_ws_send(std::forward<detail::ws_frame>(frame)), detached);
#endif
  }

  awaitable<void> coro_ws_send(detail::ws_frame&& frame)
  {
    std::unique_lock<std::mutex> lock{ws_helper_->write_queue_mutex_};
    ws_helper_->write_queue_.emplace(std::move(frame));
#ifdef WS_WSPAWN
    if (ws_helper_->write_queue_.size() == 1 && ws_handshake_) {
      lock.unlock();
      co_await coro_do_send_ws_frame();
    }
#endif
    co_return;
  }

  awaitable<void> coro_do_send_ws_frame()
  {
#ifndef WS_WSPAWN
    asio::steady_timer timer(co_await asio::this_coro::executor, asio::steady_timer::time_point::max());
    //if (ws_helper_->write_queue_.empty())
    co_await timer.async_wait(asio::bind_cancellation_slot(cancel_signal_.slot(), use_nothrow_awaitable));
#endif

    for(;;){ //do_send_ws_frame
        auto& frame = get_frame();
        std::ostream os{&ws_helper_->buffer_};
        // opcode
        auto opcode = static_cast<std::uint8_t>(frame.opcode) | 0x80;
        os.write(reinterpret_cast<char*>(&opcode), 1);
        // length
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
            // payload
            auto& payload = frame.payload;
            os.write(payload.data(), payload.size());
        }

        //do_write_ws()
        auto [code, bytes_transferred] = co_await asio::async_write(socket_, ws_helper_->buffer_, use_nothrow_awaitable);
        detail::unused(bytes_transferred);
        if (code) {
            std::cerr << "close : " << code.what() << std::endl;
            co_await close();
            co_return;
        } else {
            std::unique_lock<std::mutex> lock{ws_helper_->write_queue_mutex_};
            ws_helper_->write_queue_.pop();
            if (!ws_helper_->write_queue_.empty()) {
                lock.unlock();
                //do_send_ws_frame();
            }
            else {
                lock.unlock();// DO NOT keep lock
#ifndef WS_WSPAWN
                co_await timer.async_wait(asio::bind_cancellation_slot(cancel_signal_.slot(), use_nothrow_awaitable));
#else
            //co_return;
#endif
            }
        }
    }
  }

  void do_read_some() {
    auto buffer = context_.req().buffer();
    socket_.async_read_some(
        asio::buffer(buffer.first, buffer.second),
        [this, self = this->shared_from_this()](const boost::system::error_code& code, std::size_t bytes_transferred) {
          if (code) {
            if (code == asio::error::eof) {
              boost::system::error_code shutdown_code;
              socket().shutdown(asio::ip::tcp::socket::shutdown_both, shutdown_code);
            }
            return;
          }

          reply_str_.reserve(4096);
          const auto finished = parse(bytes_transferred);
          reply(finished);
        });
  }

  bool parse(std::size_t bytes) {
    const auto parse_code = context_.req().parse(bytes);
    // = 0 success = -1 error = -2 not complete = -3 pipeline
    switch (parse_code) {
      case 0:
        handle();
        return true;
      case -1:
        reply_str_.append(make_reply_str(400));
        return true;
      case -3:
        handle();
        for (;;) {
          const auto code = context_.req().parse(0);
          if (code == 0) {
            handle();
            return true;
          } else if (code == -1) {
            reply_str_.append(make_reply_str(400));
            return true;
          } else if (code == -2) {
            handle();
            return false;
          } else {
            handle();
          }
        }
      case -2:
        return false;
      default:
        do_read();
        break;
    }
    return true;
  }

  //  void handle() {
  //    assert(handler_);
  //    auto& req = context_.req();
  //    auto& res = context_.res();
  //    res.minor_version(req.minor_version());
  //    handler_(context_);
  //    if (req.websocket() && !ws_helper_) {
  //      ws_helper_ = std::make_unique<ws_helper>();
  //      ws_helper_->websocket_ = context_.websocket_ptr();
  //    }
  //    if (!res.is_stream()) {
  //      res.to_string(reply_str_);
  //    }
  //  }

    void reply(bool finished) {
      asio::async_write(
          socket_, asio::buffer(reply_str_),
          [this, finished, self = this->shared_from_this()](const boost::system::error_code& code, std::size_t bytes_transferred) {
            detail::unused(bytes_transferred);
            if (code) {
              return;
            }

            reply_str_.clear();
            if (ws_helper_) {
              ws_handshake_ = true;
              ws_helper_->websocket_->emit(detail::ws_event::open);
              do_read_ws_header();
            } else {
              if (finished) {
                context_.reset();
              }
              do_read();
            }
          });
    }


  void do_read_ws_header() {
    asio::async_read(
        socket_, asio::buffer(ws_helper_->ws_reader_.header),
        [this, self = this->shared_from_this()](const boost::system::error_code& code, std::size_t bytes_transferred) {
          detail::unused(bytes_transferred);
          if (code) {
            close();
          } else {
            auto& reader = ws_helper_->ws_reader_;
            reader.fin = reader.header[0] & 0x80;
            reader.opcode = static_cast<detail::ws_opcode>(reader.header[0] & 0xf);
            reader.has_mask = reader.header[1] & 0x80;
            reader.length = reader.header[1] & 0x7f;
            if (reader.length == 126) {
              do_read_ws_length_and_mask(2);
            } else if (reader.length == 127) {
              do_read_ws_length_and_mask(8);
            } else {
              do_read_ws_length_and_mask(0);
            }
          }
        });
  }

  void do_read_ws_length_and_mask(std::size_t bytes) {
    auto& reader = ws_helper_->ws_reader_;
    const auto length = bytes + (reader.has_mask ? 4 : 0);
    if (length == 0) {
      handle_ws();
    } else {
      reader.length_mask_buffer.resize(length);
      asio::async_read(
          socket_, asio::buffer(reader.length_mask_buffer.data(), length),
          [bytes, this, self = this->shared_from_this()](const boost::system::error_code& code, std::size_t bytes_transferred) {
            detail::unused(bytes_transferred);
            if (code) {
              close();
            } else {
              auto& reader = ws_helper_->ws_reader_;
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
              do_read_ws_payload();
            }
          });
    }
  }

  void do_read_ws_payload() {
    auto& reader = ws_helper_->ws_reader_;
    const std::size_t length{reader.payload_buffer.size()};
    reader.payload_buffer.resize(reader.length + length);
    asio::async_read(
        socket_, asio::buffer(reader.payload_buffer.data() + length, reader.length),
        [this, self = this->shared_from_this()](const boost::system::error_code& code, std::size_t bytes_transferred) {
          detail::unused(bytes_transferred);
          if (code) {
            close();
          } else {
            auto& reader = ws_helper_->ws_reader_;
            auto& payload_buffer = reader.payload_buffer;
            if (reader.has_mask) {
              for (std::size_t i{0}; i < reader.length; ++i) {
                payload_buffer[i] ^= reader.mask[i % 4];
              }
            }
            handle_ws();
          }
        });
  }

  void handle_ws() {
    auto& reader = ws_helper_->ws_reader_;
    switch (reader.opcode) {
      case detail::ws_opcode::continuation:
        reader.last_fin = false;
        break;
      case detail::ws_opcode::text:
      case detail::ws_opcode::binary: {
        if (reader.fin) {
          reader.last_fin = true;
          auto& payload_buffer = reader.payload_buffer;
          ws_helper_->websocket_->emit(detail::ws_event::msg, {payload_buffer.data(), payload_buffer.size()});
          payload_buffer.clear();
        } else {
          reader.last_fin = false;
        }
        break;
      }
      case detail::ws_opcode::close:
        close();
        return;
      case detail::ws_opcode::ping:
        reply_ws_pong();
        break;
      default:
        break;
    }
    do_read_ws_header();
  }

  void reply_ws_pong() {
    detail::ws_frame frame;
    frame.opcode = detail::ws_opcode::pong;
    send_ws_frame(std::move(frame));
  }

  void send_ws_frame(detail::ws_frame&& frame) {
    std::unique_lock<std::mutex> lock{ws_helper_->write_queue_mutex_};
    ws_helper_->write_queue_.emplace(std::move(frame));

    if (ws_helper_->write_queue_.size() == 1 && ws_handshake_) {
      lock.unlock();
      do_send_ws_frame();
    }
  }

  void do_send_ws_frame() {
    auto& frame = get_frame();
    std::ostream os{&ws_helper_->buffer_};
    // opcode
    auto opcode = static_cast<std::uint8_t>(frame.opcode) | 0x80;
    os.write(reinterpret_cast<char*>(&opcode), 1);
    // length
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
      // payload
      auto& payload = frame.payload;
      os.write(payload.data(), payload.size());
    }

    do_write_ws();
  }

  void do_write_ws() {
    asio::async_write(
        socket_, ws_helper_->buffer_,
        [this, self = this->shared_from_this()](const boost::system::error_code& code, std::size_t bytes_transferred) {
          detail::unused(bytes_transferred);
          if (code) {
            close();
          } else {
            std::unique_lock<std::mutex> lock{ws_helper_->write_queue_mutex_};
            ws_helper_->write_queue_.pop();
            if (!ws_helper_->write_queue_.empty()) {
              lock.unlock();
              do_send_ws_frame();
            }
          }
        });
  }

  detail::ws_frame& get_frame() {
    std::unique_lock<std::mutex> lock{ws_helper_->write_queue_mutex_};
    //assert(!ws_helper_->write_queue_.empty());
    return ws_helper_->write_queue_.front();
  }

  std::string make_reply_str(unsigned status) const {
    return std::string{detail::utils::get_response_line(1000 + status)};
  }

  struct ws_helper final {
    std::shared_ptr<websocket> websocket_;
    asio::streambuf buffer_;
    detail::ws_reader ws_reader_;
    std::queue<detail::ws_frame> write_queue_;
    std::mutex write_queue_mutex_;
  };

  _Socket socket_;
  context context_;
  //std::function<void(context&)> handler_;
  std::function<awaitable<void>(context&)> handler_;
  std::string reply_str_;
  bool ws_handshake_{false};
  std::unique_ptr<ws_helper> ws_helper_;
  asio::cancellation_signal cancel_signal_;
};

template <typename _Socket = http_socket>
class connection final : public base_connection<_Socket, connection<_Socket>>, safe_noncopyable {
 public:
  template <typename... _Args>
  connection(_Args&&... args) noexcept : base_connection<_Socket, connection<_Socket>>{std::forward<_Args>(args)...} {}

  asio::ip::tcp::socket& socket() noexcept { return this->socket_; }

  void do_read_real() {
    co_spawn(this->socket_.get_executor(), this->coro_http(this->shared_from_this()), detached);
    //co_spawn(this->socket_.get_executor(), this->cancel_slot(), detached);

    //this->do_read_some();
  }
};

#ifdef ENABLE_HTTPS
template <>
class connection<https_socket> final : public base_connection<https_socket, connection<https_socket>>,
                                       safe_noncopyable {
 public:
  template <typename... _Args>
  connection(_Args&&... args) noexcept : base_connection{std::forward<_Args>(args)...} {}

  asio::ip::tcp::socket& socket() noexcept { return socket_.next_layer(); }

  void do_read_real() {
    if (has_handshake_) {
      do_read_some();
    } else {
      do_handshake();
    }
  }

 private:
  void do_handshake() {
    socket_.async_handshake(asio::ssl::stream_base::server,
                            [this, self = this->shared_from_this()](const boost::system::error_code& code) {
                              if (code) {
                                close();
                              } else {
                                has_handshake_ = true;
                                do_read_some();
                              }
                            });
  }

  bool has_handshake_{false};
};
#endif  // ENABLE_HTTPS

}  // namespace detail
}  // namespace http
}  // namespace cue

#endif  // CUEHTTP_CONNECTION_HPP_
