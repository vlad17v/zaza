#include "transport/udp_transport.hpp"
#include "transport/tcp_transport.hpp"
#include "transport/tls_transport.hpp"
#include "transport/dtls_transport.hpp"
#include "transport/ssl_context_factory.hpp"
#include "core/turn_dispatcher.hpp"

#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <csignal>
#include <memory>

namespace asio = boost::asio;

static asio::io_context* g_ioc = nullptr;

static void signal_handler(int) {
    std::cout << "\n[turn] shutting down...\n";
    if (g_ioc) g_ioc->stop();
}

int main(int argc, char* argv[]) {
    std::string cert_file     = "../backend/certs/cert.pem";
    std::string key_file      = "../backend/certs/key.pem";
    std::string shared_secret = "turn_shared_secret";
    std::string realm         = "chat.example.com";
    std::string relay_addr    = "127.0.0.1";
    uint16_t    port_min      = 49152;
    uint16_t    port_max      = 65535;

    if (argc >= 3) {
        cert_file = argv[1];
        key_file  = argv[2];
    }
    if (argc >= 4) shared_secret = argv[3];

    asio::io_context ioc;
    g_ioc = &ioc;

    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    auto dispatcher = std::make_shared<TurnDispatcher>(
        ioc, realm, shared_secret, relay_addr, port_min, port_max);

    transport::UdpTransport udp(ioc, "0.0.0.0", 3478);
    transport::TcpTransport tcp_t(ioc, "0.0.0.0", 3478);

    auto tls_ctx      = transport::makeTlsContext(cert_file, key_file);
    SSL_CTX* dtls_raw = transport::makeDtlsContext(cert_file, key_file);
    struct DtlsGuard {
        SSL_CTX* ctx;
        ~DtlsGuard() { SSL_CTX_free(ctx); }
    } dtls_guard{dtls_raw};

    transport::TlsTransport  tls(ioc, tls_ctx, "0.0.0.0", 5349);
    transport::DtlsTransport dtls(ioc, dtls_raw, "0.0.0.0", 5349);

    auto make_handler = [&dispatcher](transport::ITransport& t) {
        return [&dispatcher, transport_ptr = &t](const uint8_t*             data,
                                                size_t                     size,
                                                const transport::Endpoint& from) {
            dispatcher->onPacket(data, size, from, *transport_ptr);
        };
    };

    udp.onReceive(make_handler(udp));
    tcp_t.onReceive(make_handler(tcp_t));
    tls.onReceive(make_handler(tls));
    dtls.onReceive(make_handler(dtls));

    udp.run();
    tcp_t.run();
    tls.run();
    dtls.run();

    std::cout << "[turn] listening on UDP/TCP :3478, TLS/DTLS :5349\n";
    std::cout << "[turn] realm: " << realm << "\n";
    std::cout << "[turn] relay: " << relay_addr
              << " ports " << port_min << "-" << port_max << "\n";

    ioc.run();

    std::cout << "[turn] stopped\n";
    return 0;
}