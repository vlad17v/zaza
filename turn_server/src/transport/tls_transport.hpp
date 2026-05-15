#pragma once

#include "transport.hpp"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <array>

namespace asio = boost::asio;
namespace ssl  = boost::asio::ssl;
using tcp      = asio::ip::tcp;

namespace transport {

class TlsSession;

class TlsTransport : public ITransport {
public:
    TlsTransport(asio::io_context&  ioc,
                 ssl::context&      ssl_ctx,
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
    ssl::context&     ssl_ctx_;
    tcp::acceptor     acceptor_;
    ReceiveCallback   on_receive_;

    std::mutex                                              sessions_mutex_;
    std::unordered_map<std::string,
                       std::shared_ptr<TlsSession>>         sessions_;
};

}