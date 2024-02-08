#pragma once

#include "../error_code.hpp"
#include "../udp.hpp"

namespace nexus::quic {

// enable the socket options necessary for a quic client or server
void prepare_socket(udp::socket& sock, bool is_server, error_code& ec);

} // namespace nexus::quic
