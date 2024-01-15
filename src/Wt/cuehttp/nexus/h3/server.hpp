#pragma once

#include <cuehttp/nexus/udp.hpp>
#include <cuehttp/nexus/ssl.hpp>
#include <cuehttp/nexus/quic/server.hpp>

namespace nexus::h3 {

class acceptor;
class server_connection;
class stream;

/// an HTTP/3 server capable of managing one or more UDP sockets via
/// class acceptor
class server {
  friend class acceptor;
  quic::detail::engine_impl engine;
 public:
  /// the polymorphic executor type, boost::asio::any_io_executor
  using executor_type = quic::detail::engine_impl::executor_type;

  /// construct the server with its associated executor
  explicit server(const executor_type& ex);

  /// construct the server with its associated executor and transport settings
  server(const executor_type& ex, const quic::settings& s);

  /// return the associated io executor
  executor_type get_executor() const;

  /// stop accepting new connections and streams entirely, and mark existing
  /// connections as 'going away'. each associated acceptor is responsible for
  /// closing its own socket
  void close();
};

/// an HTTP/3 acceptor that owns a UDP socket and uses it to accept and
/// service incoming connections
class acceptor {
  friend class server_connection;
  quic::detail::socket_impl impl;
 public:
  /// the polymorphic executor type, boost::asio::any_io_executor
  using executor_type = quic::detail::socket_impl::executor_type;

  /// construct the acceptor, taking ownership of a bound UDP socket
  acceptor(server& s, udp::socket&& socket, ssl::context& ctx);

  /// construct the acceptor and bind a UDP socket to the given endpoint
  acceptor(server& s, const udp::endpoint& endpoint, ssl::context& ctx);

  /// return the associated io executor
  executor_type get_executor() const;

  /// return the socket's locally-bound address/port
  udp::endpoint local_endpoint() const;

  /// start receiving packets on the socket. incoming connections can be
  /// accepted with accept()/async_accept(). if the queue of unaccepted
  /// connections reaches 'backlog' in size, new connections are rejected
  void listen(int backlog);

  /// accept an incoming connection whose TLS handshake has completed
  /// successfully
  template <typename CompletionToken> // void(error_code)
  decltype(auto) async_accept(server_connection& conn,
                              CompletionToken&& token) {
    return impl.async_accept(conn, std::forward<CompletionToken>(token));
  }

  /// accept an incoming connection whose TLS handshake has completed
  /// successfully
  void accept(server_connection& conn, error_code& ec);
  /// \overload
  void accept(server_connection& conn);

  /// close the socket, along with any related connections
  void close();
};

/// an HTTP/3 connection that can accept incoming streams and push associated
/// outgoing streams
class server_connection {
  friend class acceptor;
  friend class stream;
  friend class quic::detail::socket_impl;
  quic::detail::connection_impl impl;
 public:
  /// the polymorphic executor type, boost::asio::any_io_executor
  using executor_type = quic::detail::connection_impl::executor_type;

  /// construct a server-side connection for use with accept()
  explicit server_connection(acceptor& a) : impl(a.impl) {}

  /// return the associated io executor
  executor_type get_executor() const;

  /// determine whether the connection is open
  bool is_open() const;

  /// return the connection id if open
  quic::connection_id id(error_code& ec) const;
  /// \overload
  quic::connection_id id() const;

  /// return the remote's address/port if open
  udp::endpoint remote_endpoint(error_code& ec) const;
  /// \overload
  udp::endpoint remote_endpoint() const;

  /// accept an incoming stream
  template <typename CompletionToken> // void(error_code)
  decltype(auto) async_accept(stream& s, CompletionToken&& token) {
    return impl.async_accept<stream>(s, std::forward<CompletionToken>(token));
  }
  /// \overload
  void accept(stream& s, error_code& ec);
  /// \overload
  void accept(stream& s);

  // TODO: push stream

  /// send a GOAWAY frame and stop initiating or accepting new streams
  void go_away(error_code& ec);
  /// \overload
  void go_away();

  /// close the connection, along with any related streams
  void close(error_code& ec);
  /// \overload
  void close();
};

} // namespace nexus::h3
