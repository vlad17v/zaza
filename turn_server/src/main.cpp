#include "transport/udp_transport.hpp"
#include "transport/tcp_transport.hpp"
#include "transport/tls_transport.hpp"
#include "transport/dtls_transport.hpp"
#include "transport/ssl_context_factory.hpp"
#include "message/parser.hpp"
#include "message/builder.hpp"

#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <memory>

namespace asio = boost::asio;

int main(int argc, char* argv[]) {
    std::string cert_file = "tls/cert.pem";
    std::string key_file  = "tls/key.pem";

    if (argc >= 3) {
        cert_file = argv[1];
        key_file  = argv[2];
    }

    asio::io_context ioc;

    transport::UdpTransport udp(ioc, "0.0.0.0", 3478);
    transport::TcpTransport tcp_t(ioc, "0.0.0.0", 3478);

    auto tls_ctx  = transport::makeTlsContext(cert_file, key_file);

    // DTLS_CTX владеем вручную
    SSL_CTX* dtls_ctx_raw = transport::makeDtlsContext(cert_file, key_file);
    struct DtlsCtxGuard {
        SSL_CTX* ctx;
        ~DtlsCtxGuard() { SSL_CTX_free(ctx); }
    } dtls_guard{dtls_ctx_raw};

    transport::TlsTransport  tls(ioc, tls_ctx,      "0.0.0.0", 5349);
    transport::DtlsTransport dtls(ioc, dtls_ctx_raw, "0.0.0.0", 5349);

    auto make_handler = [](transport::ITransport& t) {
        return [&t](const uint8_t* data, size_t size,
                    const transport::Endpoint& from) {
            message::TurnMessage msg;
            auto result = message::parse(data, size, msg);

            if (result != message::ParseResult::Ok) {
                std::cerr << "[turn] parse failed: "
                          << static_cast<int>(result) << "\n";
                return;
            }

            std::cout << "[turn] method=" << static_cast<int>(msg.method)
                      << " class=" << static_cast<int>(msg.msg_class)
                      << " from " << from.address << ":" << from.port << "\n";

            auto resp = message::make400(msg.transaction_id);
            t.send(resp, from);
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
    ioc.run();

    return 0;
}