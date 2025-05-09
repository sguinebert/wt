#pragma once

#include "../error_code.hpp"
#include "../udp.hpp"

namespace nexus::quic {
using udp_socket = boost::asio::basic_datagram_socket<boost::asio::ip::udp,
                                          boost::asio::io_context::executor_type>;
// enable the socket options necessary for a quic client or server
void prepare_socket(udp_socket& sock, bool is_server, error_code& ec);

} // namespace nexus::quic
