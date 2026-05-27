#include "transport/udp_transport.hpp"
#include "transport/tcp_transport.hpp"
#include "transport/tls_transport.hpp"
#include "transport/dtls_transport.hpp"
#include "transport/ssl_context_factory.hpp"
#include "core/turn_dispatcher.hpp"
#include "log/logger.hpp"
#include "config/config.hpp"

#include <boost/asio.hpp>
#include <string>
#include <csignal>
#include <memory>

namespace asio = boost::asio;

static asio::io_context* g_ioc = nullptr;

static void signal_handler(int) {
    LOG("[turn] shutting down...");
    if (g_ioc) g_ioc->stop();
}

int main(int argc, char* argv[]) {
    std::string env_file = (argc >= 2) ? argv[1] : "../turn.env";
    try {
        Config::instance().load(env_file);
    } catch (const std::exception& e) {
        std::cerr << "[turn] config: " << e.what() << " — using defaults\n";
    }

    std::string cert_file     = CFG_DEF("CERT_FILE",     "../backend/certs/cert.pem");
    std::string key_file      = CFG_DEF("KEY_FILE",      "../backend/certs/key.pem");
    std::string shared_secret = CFG_DEF("SHARED_SECRET", "turn_shared_secret");
    std::string realm         = CFG_DEF("REALM",         "chat.example.com");
    std::string relay_addr    = CFG_DEF("RELAY_ADDR",    "127.0.0.1");
    uint16_t    port_min      = CFG_INT("PORT_MIN",       49152);
    uint16_t    port_max      = CFG_INT("PORT_MAX",       65535);
    std::string log_file      = CFG_DEF("LOG_FILE",       "");

    if (!log_file.empty())
        Logger::instance().setFile(log_file);

    asio::io_context ioc;
    g_ioc = &ioc;

    auto work = asio::make_work_guard(ioc);

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
        return [&dispatcher, transport_ptr = &t](const uint8_t* data,
                                                  size_t         size,
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

    LOG("[turn] listening on UDP/TCP :3478, TLS/DTLS :5349");
    LOG("[turn] realm: " + realm);
    LOG("[turn] relay: " + relay_addr +
        " ports " + std::to_string(port_min) +
        "-"       + std::to_string(port_max));

    ioc.run();

    LOG("[turn] stopped");
    return 0;
}