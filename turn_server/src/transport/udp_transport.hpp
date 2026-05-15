#pragma once

#include "transport.hpp"
#include <boost/asio.hpp>
#include <memory>

namespace asio = boost::asio;
using udp      = asio::ip::udp;

namespace transport {

class UdpTransport : public ITransport {
public:
    UdpTransport(asio::io_context&  ioc,
                 const std::string& address,
                 uint16_t           port);

    void run()                                  override;
    void stop()                                 override;
    void send(const std::vector<uint8_t>& data,
              const Endpoint&             to)   override;
    void onReceive(ReceiveCallback callback)     override;

private:
    void receive();

    asio::io_context& ioc_;
    udp::socket       socket_;
    ReceiveCallback   on_receive_;

    static constexpr size_t kMaxPacketSize = 65536;
    std::vector<uint8_t>    recv_buf_;
    udp::endpoint           remote_endpoint_;
};

}