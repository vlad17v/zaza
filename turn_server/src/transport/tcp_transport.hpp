#pragma once

#include "transport.hpp"
#include <boost/asio.hpp>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <array>

namespace asio = boost::asio;
using tcp      = asio::ip::tcp;

namespace transport {

class TcpSession;

class TcpTransport : public ITransport {
public:
    TcpTransport(asio::io_context&  ioc,
                 const std::string& address,
                 uint16_t           port);

    void run()                                  override;
    void stop()                                 override;
    void send(const std::vector<uint8_t>& data,
              const Endpoint&             to)   override;
    void onReceive(ReceiveCallback callback)     override;

    void removeSession(const std::string& key);

private:
    void accept();

    asio::io_context& ioc_;
    tcp::acceptor     acceptor_;
    ReceiveCallback   on_receive_;

    std::mutex                                              sessions_mutex_;
    std::unordered_map<std::string,
                       std::shared_ptr<TcpSession>>         sessions_;
};

}