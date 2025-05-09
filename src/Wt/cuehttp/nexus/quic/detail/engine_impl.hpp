#pragma once

#include <memory>
#include <mutex>

#include <Wt/AsioWrapper/asio.hpp>
#include "../../../detail/engines.hpp"

//#include <boost/asio/steady_timer.hpp>

#include "../settings.hpp"

struct lsquic_engine;
struct lsquic_conn;
struct lsquic_stream;
struct lsquic_out_spec;

namespace nexus::quic::detail {

struct connection_impl;
struct stream_impl;
struct socket_impl;

struct engine_deleter { void operator()(lsquic_engine* e) const; };
using lsquic_engine_ptr = std::unique_ptr<lsquic_engine, engine_deleter>;

struct engine_impl {
  using executor_type = asio::io_context::executor_type;
  using udp_socket = asio::basic_datagram_socket<asio::ip::udp, executor_type>;
  mutable std::mutex mutex;
  const Wt::http::detail::engines *engine_ = nullptr;
  const executor_type *executor_ = nullptr;
  asio::steady_timer timer;
  lsquic_engine_ptr handle;
  // pointer to client socket or null if server
  socket_impl* client;
  uint32_t max_streams_per_connection;
  bool is_http;

  void process(std::unique_lock<std::mutex>& lock);
  void reschedule(std::unique_lock<std::mutex>& lock);
  void on_timer();

  engine_impl(const Wt::http::detail::engines* engine, socket_impl* client,
              const settings* s, unsigned flags);
  engine_impl(const executor_type* ex, socket_impl* client,
              const settings* s, unsigned flags);
  ~engine_impl();


  executor_type get_executor() const {
      return executor_
                 ? *executor_
                 : engine_->get().get_executor();  // ERROR
  }
  void close();

  int send_packets(const lsquic_out_spec *specs, unsigned n_specs);

  stream_impl* on_new_stream(connection_impl& c, lsquic_stream* stream);
};

} // namespace nexus::quic::detail
