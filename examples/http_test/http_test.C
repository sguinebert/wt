/*
 * Minimal test server for HTTP/1.1, HTTP/2, and HTTP/3.
 *
 * Usage:
 *   ./http_test
 *
 * Then test with:
 *   curl -k https://localhost:8443/              # HTTP/1.1 over TLS
 *   curl -k --http2 https://localhost:8443/      # HTTP/2
 *   curl -k --http3-only https://localhost:8444/  # HTTP/3 (needs curl with HTTP/3 + lsquic built against BoringSSL)
 */

#include <Wt/cuehttp/server.hpp>
#include <Wt/cuehttp/server_h2.hpp>
#ifdef WT_WITH_HTTP3
#include <Wt/cuehttp/h3_server.hpp>
#endif

#include <iostream>
#include <fstream>
#include <csignal>

namespace whttp = Wt::http;
using boost::asio::awaitable;

static whttp::detail::engines* g_engine = nullptr;

void signal_handler(int) {
    if (g_engine) g_engine->stop();
}

int main() {
    const std::string cert_file = "test_cert.pem";
    const std::string key_file  = "test_key.pem";

    // Generate self-signed cert if not present
    {
        std::ifstream f(cert_file);
        if (!f.good()) {
            std::cout << "Generating self-signed certificate...\n";
            int ret = std::system(
                "openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 "
                "-keyout test_key.pem -out test_cert.pem -days 365 -nodes "
                "-subj '/CN=localhost' 2>/dev/null");
            if (ret != 0) {
                std::cerr << "Failed to generate certificate. Is openssl installed?\n";
                return 1;
            }
        }
    }

    // Shared handler for all protocols
    auto handler = [](whttp::context& ctx) -> awaitable<void> {
        auto method = ctx.req().method();
        auto path   = ctx.req().path();
        auto host   = ctx.req().host();

        // Detect protocol: H2/H3 store :authority pseudo-header
        std::string protocol = "HTTP/1.1";
        auto auth = ctx.req().get(":authority");
        if (!auth.empty()) {
            protocol = "HTTP/2+";
        }

        std::string body =
            "Hello from cuehttp!\n"
            "Protocol: " + protocol + "\n"
            "Method:   " + std::string(method) + "\n"
            "Path:     " + std::string(path) + "\n"
            "Host:     " + std::string(host) + "\n";

        ctx.status(200);
        ctx.res().type("text/plain");
        ctx.res().body(body);
        ctx.flush();

        std::cout << "[" << protocol << "] " << method << " " << path << "\n";

        co_return;
    };

    // Engine pool (2 threads for testing)
    auto& engine = whttp::detail::engines::engine(2);
    g_engine = &engine;

    // Signal handling for clean shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // HTTP/2 server on port 8443 (also serves HTTP/1.1 over TLS)
    auto h2_srv = whttp::https_h2::create_server(handler, &engine, key_file, cert_file);
    h2_srv.listen(8443);
    std::cout << "HTTP/1.1+2 server listening on https://localhost:8443\n";

#ifdef WT_WITH_HTTP3
    // HTTP/3 server on port 8444 (UDP)
    whttp::h3_server h3_srv{handler, &engine, cert_file, key_file};
    h3_srv.listen(8444);
    std::cout << "HTTP/3 server listening on https://localhost:8444 (UDP)\n";
#else
    std::cout << "HTTP/3 not available (lsquic not linked)\n";
#endif

    std::cout << "\nTest with:\n"
              << "  curl -k https://localhost:8443/           # HTTP/1.1\n"
              << "  curl -k --http2 https://localhost:8443/   # HTTP/2\n"
#ifdef WT_WITH_HTTP3
              << "  curl -k --http3-only https://localhost:8444/  # HTTP/3\n"
#endif
              << "\nPress Ctrl+C to stop.\n\n";

    engine.run();

#ifdef WT_WITH_HTTP3
    h3_srv.close();
#endif
    std::cout << "\nServer stopped.\n";
    return 0;
}
