#include "transport/udp_transport.hpp"
#include "transport/tcp_transport.hpp"
#include "message/parser.hpp"
#include "message/builder.hpp"

#include <boost/asio.hpp>
#include <iostream>

namespace asio = boost::asio;

int main() {
    asio::io_context ioc;

    transport::UdpTransport udp(ioc, "0.0.0.0", 3478);
    transport::TcpTransport tcp_t(ioc, "0.0.0.0", 3478);

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

    udp.run();
    tcp_t.run();

    std::cout << "[turn] listening on UDP/TCP :3478\n";
    ioc.run();

    return 0;
}