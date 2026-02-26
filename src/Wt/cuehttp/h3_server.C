/*
 * h3_server implementation — compiled as part of whttp with BoringSSL prefix.
 *
 * All SSL_CTX operations happen here (using BSSL_-prefixed BoringSSL symbols),
 * ensuring no conflict with system OpenSSL used by the HTTP/1.1+2 stack.
 */

#include "h3_server.hpp"
#include "detail/h3_stream_adapter.hpp"
#include "nexus/h3/server.hpp"
#include "nexus/ssl.hpp"
#include "nexus/udp.hpp"

namespace Wt {
namespace http {

struct h3_server::impl {
    std::function<awaitable<void>(context&)> handler;
    detail::engines* engine;
    nexus::ssl::context ssl_ctx{nexus::ssl::context::tlsv13};
    std::unique_ptr<nexus::h3::server> server;
    std::unique_ptr<nexus::h3::acceptor> acceptor;

    impl(std::function<awaitable<void>(context&)> h,
         detail::engines* e,
         const std::string& cert_file,
         const std::string& key_file)
        : handler(std::move(h)), engine(e)
    {
        ssl_ctx.set_options(nexus::ssl::context::default_workarounds
                          | nexus::ssl::context::no_sslv2
                          | nexus::ssl::context::no_sslv3
                          | nexus::ssl::context::no_tlsv1
                          | nexus::ssl::context::no_tlsv1_1
                          | nexus::ssl::context::no_tlsv1_2);
        ssl_ctx.use_certificate_chain_file(cert_file);
        ssl_ctx.use_private_key_file(key_file, nexus::ssl::context::pem);
    }

    awaitable<void> accept_loop() {
        for (;;) {
            auto conn = std::make_shared<nexus::h3::server_connection>(*acceptor);
            boost::system::error_code ec;
            co_await acceptor->async_accept(*conn,
                asio::redirect_error(asio::use_awaitable, ec));
            if (ec) break;

            co_spawn(conn->get_executor(),
                     connection_loop(conn),
                     asio::detached);
        }
    }

    awaitable<void> connection_loop(std::shared_ptr<nexus::h3::server_connection> conn) {
        for (;;) {
            auto stream = std::make_shared<nexus::h3::stream>(*conn);
            boost::system::error_code ec;
            co_await conn->async_accept(*stream,
                asio::redirect_error(asio::use_awaitable, ec));
            if (ec) break;

            co_spawn(stream->get_executor(),
                     stream_handler(stream),
                     asio::detached);
        }
    }

    awaitable<void> stream_handler(std::shared_ptr<nexus::h3::stream> stream) {
        nexus::h3::fields req_fields;
        boost::system::error_code ec;
        co_await stream->async_read_headers(req_fields,
            asio::redirect_error(asio::use_awaitable, ec));
        if (ec) co_return;

        co_await detail::handle_h3_stream(*stream, req_fields, handler);
    }
};

h3_server::h3_server(std::function<awaitable<void>(context&)> handler,
                     detail::engines* engine,
                     const std::string& cert_file,
                     const std::string& key_file) noexcept
    : pimpl_(std::make_unique<impl>(std::move(handler), engine, cert_file, key_file))
{
}

h3_server::~h3_server() = default;

h3_server& h3_server::listen(unsigned port) {
    return listen(port, "::");
}

h3_server& h3_server::listen(unsigned port, const std::string& host) {
    nexus::udp::endpoint endpoint{
        boost::asio::ip::make_address(host),
        static_cast<unsigned short>(port)};

    pimpl_->server = std::make_unique<nexus::h3::server>(pimpl_->engine);
    pimpl_->acceptor = std::make_unique<nexus::h3::acceptor>(
        *pimpl_->server, endpoint, pimpl_->ssl_ctx);
    pimpl_->acceptor->listen(128);

    co_spawn(pimpl_->engine->get(), pimpl_->accept_loop(), asio::detached);
    return *this;
}

void h3_server::close() {
    if (pimpl_->acceptor) pimpl_->acceptor->close();
    if (pimpl_->server) pimpl_->server->close();
}

}  // namespace http
}  // namespace Wt
