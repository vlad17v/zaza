#include "udp_transport.hpp"
#include <iostream>

namespace transport {

UdpTransport::UdpTransport(asio::io_context&  ioc,
                            const std::string& address,
                            uint16_t           port)
    : ioc_(ioc)
    , socket_(ioc, udp::endpoint(asio::ip::make_address(address), port))
    , recv_buf_(kMaxPacketSize)
{}

void UdpTransport::onReceive(ReceiveCallback callback) {
    on_receive_ = std::move(callback);
}

void UdpTransport::run() {
    receive();
}

void UdpTransport::stop() {
    boost::system::error_code ec;
    socket_.close(ec);
}

void UdpTransport::send(const std::vector<uint8_t>& data,
                         const Endpoint&             to) {
    auto buf = std::make_shared<std::vector<uint8_t>>(data);
    udp::endpoint dest(asio::ip::make_address(to.address), to.port);
    socket_.async_send_to(
        asio::buffer(*buf), dest,
        [buf](boost::system::error_code ec, size_t) {
            if (ec)
                std::cerr << "[udp] send error: " << ec.message() << "\n";
        });
}

void UdpTransport::receive() {
    socket_.async_receive_from(
        asio::buffer(recv_buf_), remote_endpoint_,
        [this](boost::system::error_code ec, size_t bytes) {
            if (!ec && bytes > 0 && on_receive_) {
                Endpoint from{
                    remote_endpoint_.address().to_string(),
                    remote_endpoint_.port()
                };
                on_receive_(recv_buf_.data(), bytes, from);
            }
            if (!ec) receive();
        });
}

}