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

#include "detail/noncopyable.hpp"
#include "detail/engines.hpp"
#include "WHttpDllDefs.h"

#include <functional>
#include <memory>
#include <string>

namespace Wt {
namespace http {

class context;

/// Standalone HTTP/3 server (UDP-based, wraps nexus::h3).
///
/// Accepts the same handler signature as the HTTP/1.1+2 TCP server,
/// so a single handler lambda can serve all three protocols.
///
/// Usage:
///   h3_server srv{handler, &engines, "cert.pem", "key.pem"};
///   srv.listen(443);           // UDP port
///   engines.run();
///
class WHTTP_API h3_server final : safe_noncopyable {
public:
    h3_server(std::function<awaitable<void>(context&)> handler,
              detail::engines* engine,
              const std::string& cert_file,
              const std::string& key_file) noexcept;

    ~h3_server();

    h3_server& listen(unsigned port);
    h3_server& listen(unsigned port, const std::string& host);
    void close();

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;
};

}  // namespace http
}  // namespace Wt
