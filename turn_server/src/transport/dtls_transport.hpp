#pragma once

#include "transport.hpp"

#include <boost/asio.hpp>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/rand.h>

#include <memory>
#include <mutex>
#include <unordered_map>
#include <string>
#include <vector>

namespace asio = boost::asio;
using udp      = asio::ip::udp;

namespace transport {

class DtlsTransport;

class DtlsSession : public std::enable_shared_from_this<DtlsSession> {
public:
    DtlsSession(asio::io_context&    ioc,
                SSL_CTX*             ssl_ctx,
                udp::socket&         server_socket,
                const udp::endpoint& peer_endpoint,
                ReceiveCallback&     on_receive);

    ~DtlsSession();

    void start();
    void feed(const uint8_t* data, size_t size);
    void send(const std::vector<uint8_t>& data);

    const udp::endpoint& peer() const { return peer_endpoint_; }

private:
    void flush_write();

    asio::io_context& ioc_;
    SSL_CTX*          ssl_ctx_;
    udp::socket&      server_socket_;
    udp::endpoint     peer_endpoint_;
    ReceiveCallback&  on_receive_;

    SSL* ssl_   = nullptr;
    BIO* rbio_  = nullptr;
    BIO* wbio_  = nullptr;

    bool       handshake_done_ = false;
    std::mutex mutex_;

    static constexpr size_t kMaxPacket = 65536;
};

class DtlsTransport : public ITransport {
public:
    DtlsTransport(asio::io_context&  ioc,
                  SSL_CTX*           ssl_ctx,
                  const std::string& address,
                  uint16_t           port);

    ~DtlsTransport();

    void run()                                  override;
    void stop()                                 override;
    void send(const std::vector<uint8_t>& data,
              const Endpoint&             to)   override;
    void onReceive(ReceiveCallback callback)     override;

private:
    void        receive();
    std::string endpointKey(const udp::endpoint& ep) const;

    asio::io_context& ioc_;
    SSL_CTX*          ssl_ctx_;
    udp::socket       socket_;
    ReceiveCallback   on_receive_;

    std::vector<uint8_t> recv_buf_;
    udp::endpoint        remote_endpoint_;

    std::mutex                                           sessions_mutex_;
    std::unordered_map<std::string,
                       std::shared_ptr<DtlsSession>>     sessions_;
};

}