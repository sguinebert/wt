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

#ifndef CUEHTTP_SERVER_HPP_
#define CUEHTTP_SERVER_HPP_

#include <thread>
#include <functional>
#include <vector>
#include <type_traits>

//#include <boost/asio.hpp>
#include <Wt/AsioWrapper/asio.hpp>
#ifdef WT_WITH_SSL
#include <Wt/AsioWrapper/ssl.hpp>
#include <openssl/ssl.h>
#endif // WT_WITH_SSL
#include "context.hpp"
#include "detail/connection.hpp"
#include "detail/noncopyable.hpp"
#include "detail/engines.hpp"

namespace Wt {
namespace http {

template <typename _Socket, typename _Ty>
class base_server : safe_noncopyable {
public:
    base_server() noexcept = default;

    explicit base_server(std::function<awaitable<void>(context&)> handler, detail::engines* engine) noexcept
        : handler_{std::move(handler)}, engine_{engine}
    {
    }

    virtual ~base_server() = default;

    base_server(base_server&& rhs) noexcept {
        swap(rhs);
    }

    base_server& operator=(base_server&& rhs) noexcept {
        swap(rhs);
        return *this;
    }

    void swap(base_server& rhs) noexcept {
        if (this != std::addressof(rhs)) {
            std::swap(handler_, rhs.handler_);
            std::swap(acceptor_, rhs.acceptor_);
            std::swap(engine_, rhs.engine_);
        }
    }

    base_server& listen(unsigned port) {
        assert(port != 0);
        //listen_impl(asio::ip::tcp::resolver::query{std::to_string(port)});
        listen_impl("", std::to_string(port));
        return *this;
    }

    template <typename _Host>
    base_server& listen(unsigned port, _Host&& host) {
        assert(port != 0);
        //listen_impl(asio::ip::tcp::resolver::query{std::forward<_Host>(host), std::to_string(port)});
        listen_impl(std::forward<_Host>(host), std::to_string(port));
        return *this;
    }

protected:
    void listen_impl(std::string host, const std::string& service /*asio::ip::tcp::resolver::query&& query*/) {
        asio::ip::tcp::resolver resolver{engine_->get()};
        auto endpoints = resolver.resolve(host, service);
        asio::ip::tcp::endpoint endpoint = *endpoints.begin();
        //asio::ip::tcp::endpoint endpoint{*asio::ip::tcp::resolver{engine_->get()}.resolve(host, service)};
        acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(engine_->get());
        acceptor_->open(endpoint.protocol());
        acceptor_->set_option(asio::ip::tcp::acceptor::reuse_address(true));
        acceptor_->bind(endpoint);
        acceptor_->listen();
        do_accept();
    }

    void do_accept() {
        static_cast<_Ty&>(*this).do_accept_real();
    }

    std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
    std::function<awaitable<void>(context&)> handler_;
    detail::engines* engine_ = nullptr;
};

template <typename _Socket = detail::http_socket,
          template<typename> class Connection = detail::connection>
class server final : public base_server<_Socket, server<_Socket, Connection>>, safe_noncopyable {
    using base_t = base_server<_Socket, server<_Socket, Connection>>;
public:
    server() noexcept = default;

    explicit server(std::function<awaitable<void>(context&)> handler, detail::engines* engine) noexcept
        : base_t{std::move(handler), engine} {
    }

    server(server&& rhs) noexcept {
        swap(rhs);
    }

    server& operator=(server&& rhs) noexcept {
        swap(rhs);
        return *this;
    }

    void stop() {}

    void swap(server& rhs) noexcept {
        if (this != std::addressof(rhs)) {
            base_t::swap(rhs);
        }
    }

    void do_accept_real() {
        auto conn =
            std::make_shared<Connection<_Socket>>(this->handler_, this->engine_->get());
        this->acceptor_->async_accept(conn->socket(), [this, conn](const boost::system::error_code& code) {
            if (!code) {
                conn->socket().set_option(asio::ip::tcp::no_delay{true});
                conn->run();
            }

            this->do_accept();
        });
    }
};

#ifdef WT_WITH_SSL

// Function to generate a self-signed certificate and private key
// static inline std::pair<std::string, std::string> generate_self_signed_cert() {
//     std::string cert_str, key_str;

//     // Create a new EVP_PKEY for the RSA key
//     std::unique_ptr<EVP_PKEY, void(*)(EVP_PKEY*)> pkey(EVP_PKEY_new(), EVP_PKEY_free);
//     if (!pkey) {
//         throw std::runtime_error("Failed to create EVP_PKEY");
//     }

//     // Generate an RSA key using modern API
//     EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
//     if (!pctx) {
//         throw std::runtime_error("Failed to create EVP_PKEY_CTX");
//     }
//     if (EVP_PKEY_keygen_init(pctx) <= 0) {
//         EVP_PKEY_CTX_free(pctx);
//         throw std::runtime_error("Failed to initialize keygen");
//     }
//     if (EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, 2048) <= 0) {
//         EVP_PKEY_CTX_free(pctx);
//         throw std::runtime_error("Failed to set RSA key bits");
//     }
//     EVP_PKEY* pkey_raw = pkey.get();
//     if (EVP_PKEY_keygen(pctx, &pkey_raw) <= 0) {
//         EVP_PKEY_CTX_free(pctx);
//         throw std::runtime_error("Failed to generate RSA key");
//     }
//     EVP_PKEY_CTX_free(pctx);

//     // Create a new X.509 certificate
//     std::unique_ptr<X509, void(*)(X509*)> cert(X509_new(), X509_free);
//     if (!cert) {
//         throw std::runtime_error("Failed to create X509 certificate");
//     }
//     X509_set_version(cert.get(), 2); // Version 3 (index 2)
//     ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), 1);
//     X509_gmtime_adj(X509_get_notBefore(cert.get()), 0); // Valid from now
//     X509_gmtime_adj(X509_get_notAfter(cert.get()), 60 * 60 * 24 * 365); // Valid for 1 year

//     // Set the public key in the certificate
//     if (!X509_set_pubkey(cert.get(), pkey.get())) {
//         throw std::runtime_error("Failed to set public key");
//     }

//     // Set subject name (and issuer, since it's self-signed)
//     X509_NAME* name = X509_get_subject_name(cert.get());
//     if (!name) {
//         throw std::runtime_error("Failed to get subject name");
//     }
//     X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char*)"WT", -1, -1, 0);
//     X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (unsigned char*)"Wt", -1, -1, 0);
//     X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char*)"localhost", -1, -1, 0);
//     X509_set_issuer_name(cert.get(), name);

//     // Sign the certificate with the private key
//     if (!X509_sign(cert.get(), pkey.get(), EVP_sha256())) {
//         throw std::runtime_error("Failed to sign certificate");
//     }

//     // Convert certificate to PEM format
//     BIO* bio = BIO_new(BIO_s_mem());
//     if (!bio) {
//         throw std::runtime_error("Failed to create BIO");
//     }
//     std::unique_ptr<BIO, void (*)(BIO *)> cert_bio(bio, [](BIO* b) { BIO_free(b); });
//     if (!cert_bio) {
//         throw std::runtime_error("Failed to create BIO for certificate");
//     }
//     if (!PEM_write_bio_X509(cert_bio.get(), cert.get())) {
//         throw std::runtime_error("Failed to write certificate to BIO");
//     }
//     char* cert_data;
//     long cert_len = BIO_get_mem_data(cert_bio.get(), &cert_data);
//     cert_str.assign(cert_data, cert_len);

//     // Convert private key to PEM format
//     std::unique_ptr<BIO, void(*)(BIO*)> key_bio(
//         BIO_new(BIO_s_mem()),
//         [](BIO* bio) { BIO_free(bio); }
//         );
//     if (!key_bio) {
//         throw std::runtime_error("Failed to create BIO for private key");
//     }
//     if (!PEM_write_bio_PrivateKey(key_bio.get(), pkey.get(), nullptr, nullptr, 0, nullptr, nullptr)) {
//         throw std::runtime_error("Failed to write private key to BIO");
//     }
//     char* key_data;
//     long key_len = BIO_get_mem_data(key_bio.get(), &key_data);
//     key_str.assign(key_data, key_len);

//     return {cert_str, key_str};
// }

template <template<typename> class Connection>
class server<detail::https_socket, Connection> final
    : public base_server<detail::https_socket, server<detail::https_socket, Connection>>,
      safe_noncopyable {
    using base_t = base_server<detail::https_socket, server<detail::https_socket, Connection>>;
public:
    server(std::function<awaitable<void>(context&)> handler, detail::engines* engine, const std::string& key, const std::string& cert) noexcept
        : base_t{std::move(handler), engine}, ssl_context_{asio::ssl::context::sslv23} {
        if (!key.empty() && !cert.empty()) {
            try {
                ssl_context_.use_certificate_chain_file(cert);
                ssl_context_.use_private_key_file(key, asio::ssl::context::pem);
            } catch (const boost::system::error_code& e) {
                std::cerr << "Error setting up SSL context: " << e.what() << std::endl;
            }
        }

        // Configure ALPN for HTTP/2 + HTTP/1.1 negotiation
        SSL_CTX_set_alpn_select_cb(ssl_context_.native_handle(),
            [](SSL*, const unsigned char** out, unsigned char* outlen,
               const unsigned char* in, unsigned int inlen, void*) -> int {
                // Prefer h2, fall back to http/1.1
                static const unsigned char h2[]     = {2, 'h', '2'};
                static const unsigned char http11[] = {8, 'h', 't', 't', 'p', '/', '1', '.', '1'};
                if (SSL_select_next_proto(const_cast<unsigned char**>(out), outlen,
                        h2, sizeof(h2), in, inlen) == OPENSSL_NPN_NEGOTIATED)
                    return SSL_TLSEXT_ERR_OK;
                if (SSL_select_next_proto(const_cast<unsigned char**>(out), outlen,
                        http11, sizeof(http11), in, inlen) == OPENSSL_NPN_NEGOTIATED)
                    return SSL_TLSEXT_ERR_OK;
                return SSL_TLSEXT_ERR_NOACK;
            }, nullptr);
    }

    server(server&& rhs) noexcept : ssl_context_{asio::ssl::context::sslv23} {
        swap(rhs);
    }

    server& operator=(server&& rhs) noexcept {
        swap(rhs);
        return *this;
    }

    void swap(server& rhs) noexcept {
        if (this != std::addressof(rhs)) {
            base_t::swap(rhs);
            std::swap(ssl_context_, rhs.ssl_context_);
        }
    }

    void do_accept_real() {
        auto connector = std::make_shared<Connection<detail::https_socket>>(this->handler_, this->engine_->get(), ssl_context_);
        this->acceptor_->async_accept(connector->socket(), [this, connector](const boost::system::error_code& code) {
            if (!code) {
                connector->socket().set_option(asio::ip::tcp::no_delay{true});
                connector->run();
            }

            this->do_accept();
        });
    }

private:
    asio::ssl::context ssl_context_;
};
#endif // WT_WITH_SSL


//template<bool SSL = false> http_servercc;

using http_t = server<detail::http_socket>;

struct http final : safe_noncopyable {
    template <typename... _Args>
    static http_t create_server(_Args&&... args) noexcept {
        return http_t{std::forward<_Args>(args)...};
    }
};

#ifdef WT_WITH_SSL
using https_t = server<detail::https_socket>;

struct https final : safe_noncopyable {
    template <typename... _Args>
    static https_t create_server(_Args&&... args) noexcept {
        return https_t{std::forward<_Args>(args)...};
    }
};
#endif // WT_WITH_SSL

} // namespace http
} // namespace cue

#endif // CUEHTTP_SERVER_HPP_
