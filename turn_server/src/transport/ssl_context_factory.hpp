#pragma once

#include <boost/asio/ssl.hpp>
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <string>
#include <stdexcept>

namespace ssl = boost::asio::ssl;

namespace transport {

inline ssl::context makeTlsContext(const std::string& cert_file,
                                    const std::string& key_file) {
    ssl::context ctx(ssl::context::tls_server);

    ctx.set_options(
        ssl::context::default_workarounds |
        ssl::context::no_sslv2           |
        ssl::context::no_sslv3           |
        ssl::context::no_tlsv1           |
        ssl::context::no_tlsv1_1         |
        ssl::context::single_dh_use);

    ctx.use_certificate_chain_file(cert_file);
    ctx.use_private_key_file(key_file, ssl::context::pem);

    return ctx;
}

inline SSL_CTX* makeDtlsContext(const std::string& cert_file,
                                 const std::string& key_file) {
    SSL_CTX* ctx = SSL_CTX_new(DTLS_server_method());
    if (!ctx)
        throw std::runtime_error("SSL_CTX_new(DTLS) failed");

    if (SSL_CTX_use_certificate_chain_file(ctx, cert_file.c_str()) != 1)
        throw std::runtime_error("DTLS: cannot load cert: " + cert_file);

    if (SSL_CTX_use_PrivateKey_file(ctx, key_file.c_str(),
                                     SSL_FILETYPE_PEM) != 1)
        throw std::runtime_error("DTLS: cannot load key: " + key_file);

    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);

    SSL_CTX_set_cookie_generate_cb(
        ctx,
        [](SSL*, uint8_t* cookie, uint32_t* len) -> int {
            *len = 16;
            RAND_bytes(cookie, 16);
            return 1;
        });

    SSL_CTX_set_cookie_verify_cb(
        ctx,
        [](SSL*, const uint8_t*, uint32_t len) -> int {
            return len == 16 ? 1 : 0;
        });

    return ctx;
}

}