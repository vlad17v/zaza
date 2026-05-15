#include "dtls_transport.hpp"

#include <openssl/err.h>
#include <iostream>

namespace transport {

DtlsSession::DtlsSession(asio::io_context&    ioc,
                          SSL_CTX*             ssl_ctx,
                          udp::socket&         server_socket,
                          const udp::endpoint& peer_endpoint,
                          ReceiveCallback&     on_receive)
    : ioc_(ioc)
    , ssl_ctx_(ssl_ctx)
    , server_socket_(server_socket)
    , peer_endpoint_(peer_endpoint)
    , on_receive_(on_receive)
{
    rbio_ = BIO_new(BIO_s_mem());
    wbio_ = BIO_new(BIO_s_mem());

    ssl_ = SSL_new(ssl_ctx_);
    SSL_set_bio(ssl_, rbio_, wbio_);
    SSL_set_accept_state(ssl_);

    SSL_set_mtu(ssl_, 1200);
    DTLS_set_link_mtu(ssl_, 1280);
}

DtlsSession::~DtlsSession() {
    if (ssl_) SSL_free(ssl_);
}

void DtlsSession::flush_write() {
    char buf[kMaxPacket];
    int n;
    while ((n = BIO_read(wbio_, buf, sizeof(buf))) > 0) {
        auto data = std::make_shared<std::vector<uint8_t>>(buf, buf + n);
        server_socket_.async_send_to(
            asio::buffer(*data), peer_endpoint_,
            [data](boost::system::error_code, size_t) {});
    }
}

void DtlsSession::start() {
}

void DtlsSession::feed(const uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);

    BIO_write(rbio_, data, static_cast<int>(size));

    if (!handshake_done_) {
        int ret = SSL_do_handshake(ssl_);
        if (ret == 1) {
            handshake_done_ = true;
        } else {
            int err = SSL_get_error(ssl_, ret);
            if (err != SSL_ERROR_WANT_READ &&
                err != SSL_ERROR_WANT_WRITE) {
                std::cerr << "[dtls] handshake error: " << err << "\n";
                flush_write();
                return;
            }
        }
        flush_write();
        return;
    }

    std::vector<uint8_t> buf(kMaxPacket);
    int n = SSL_read(ssl_, buf.data(), static_cast<int>(buf.size()));
    if (n > 0) {
        buf.resize(n);
        Endpoint from{
            peer_endpoint_.address().to_string(),
            peer_endpoint_.port()
        };
        if (on_receive_)
            on_receive_(buf.data(), buf.size(), from);
    }

    flush_write();
}

void DtlsSession::send(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!handshake_done_) return;

    SSL_write(ssl_, data.data(), static_cast<int>(data.size()));
    flush_write();
}

DtlsTransport::DtlsTransport(asio::io_context&  ioc,
                               SSL_CTX*           ssl_ctx,
                               const std::string& address,
                               uint16_t           port)
    : ioc_(ioc)
    , ssl_ctx_(ssl_ctx)
    , socket_(ioc, udp::endpoint(asio::ip::make_address(address), port))
    , recv_buf_(65536)
{}

DtlsTransport::~DtlsTransport() {
    boost::system::error_code ec;
    socket_.close(ec);
}

void DtlsTransport::onReceive(ReceiveCallback callback) {
    on_receive_ = std::move(callback);
}

std::string DtlsTransport::endpointKey(const udp::endpoint& ep) const {
    return ep.address().to_string() + ":" + std::to_string(ep.port());
}

void DtlsTransport::run() {
    receive();
}

void DtlsTransport::stop() {
    boost::system::error_code ec;
    socket_.close(ec);
}

void DtlsTransport::send(const std::vector<uint8_t>& data,
                          const Endpoint&             to) {
    udp::endpoint dest(asio::ip::make_address(to.address), to.port);
    std::string key = endpointKey(dest);

    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(key);
    if (it != sessions_.end())
        it->second->send(data);
    else
        std::cerr << "[dtls] no session for " << key << "\n";
}

void DtlsTransport::receive() {
    socket_.async_receive_from(
        asio::buffer(recv_buf_), remote_endpoint_,
        [this](boost::system::error_code ec, size_t bytes) {
            if (!ec && bytes > 0) {
                std::string key = endpointKey(remote_endpoint_);

                std::shared_ptr<DtlsSession> session;
                {
                    std::lock_guard<std::mutex> lock(sessions_mutex_);
                    auto it = sessions_.find(key);
                    if (it == sessions_.end()) {
                        session = std::make_shared<DtlsSession>(
                            ioc_, ssl_ctx_, socket_,
                            remote_endpoint_, on_receive_);
                        sessions_[key] = session;
                        session->start();
                    } else {
                        session = it->second;
                    }
                }

                session->feed(recv_buf_.data(), bytes);
            }

            if (!ec) receive();
        });
}

}