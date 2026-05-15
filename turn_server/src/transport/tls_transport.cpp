#include "tls_transport.hpp"
#include <iostream>

namespace transport {

using SslStream = ssl::stream<tcp::socket>;

class TlsSession : public std::enable_shared_from_this<TlsSession> {
public:
    TlsSession(tcp::socket      socket,
               ssl::context&    ssl_ctx,
               ReceiveCallback& on_receive,
               TlsTransport&    owner)
        : stream_(std::move(socket), ssl_ctx)
        , on_receive_(on_receive)
        , owner_(owner)
    {
        auto ep = stream_.lowest_layer().remote_endpoint();
        key_  = ep.address().to_string() + ":" + std::to_string(ep.port());
        from_ = Endpoint{ep.address().to_string(), ep.port()};
    }

    void run() {
        stream_.async_handshake(
            ssl::stream_base::server,
            [self = shared_from_this()](boost::system::error_code ec) {
                if (ec) {
                    std::cerr << "[tls] handshake error: "
                              << ec.message() << "\n";
                    self->owner_.removeSession(self->key_);
                    return;
                }
                self->read_first_byte();
            });
    }

    void send(const std::vector<uint8_t>& data) {
        auto buf = std::make_shared<std::vector<uint8_t>>(data);
        asio::async_write(stream_, asio::buffer(*buf),
            [self = shared_from_this(), buf](
                boost::system::error_code ec, size_t) {
                if (ec) self->owner_.removeSession(self->key_);
            });
    }

    const std::string& key() const { return key_; }

private:
    void read_first_byte() {
        asio::async_read(stream_, asio::buffer(&first_byte_, 1),
            [self = shared_from_this()](boost::system::error_code ec, size_t) {
                if (ec) { self->owner_.removeSession(self->key_); return; }
                uint8_t top2 = (self->first_byte_ >> 6) & 0x03;
                if (top2 == 0x01)
                    self->read_channel_header();
                else
                    self->read_stun_header();
            });
    }

    void read_stun_header() {
        asio::async_read(stream_, asio::buffer(stun_header_buf_),
            [self = shared_from_this()](boost::system::error_code ec, size_t) {
                if (ec) { self->owner_.removeSession(self->key_); return; }
                std::vector<uint8_t> header(20);
                header[0] = self->first_byte_;
                std::copy(self->stun_header_buf_.begin(),
                          self->stun_header_buf_.end(),
                          header.begin() + 1);
                uint16_t attrs_len =
                    (static_cast<uint16_t>(header[2]) << 8) | header[3];
                self->read_stun_body(std::move(header), attrs_len);
            });
    }

    void read_stun_body(std::vector<uint8_t> header, uint16_t attrs_len) {
        auto body = std::make_shared<std::vector<uint8_t>>(20 + attrs_len);
        std::copy(header.begin(), header.end(), body->begin());
        asio::async_read(stream_,
            asio::buffer(body->data() + 20, attrs_len),
            [self = shared_from_this(), body](
                boost::system::error_code ec, size_t) {
                if (ec) { self->owner_.removeSession(self->key_); return; }
                if (self->on_receive_)
                    self->on_receive_(body->data(), body->size(), self->from_);
                self->read_first_byte();
            });
    }

    void read_channel_header() {
        asio::async_read(stream_, asio::buffer(chan_header_buf_),
            [self = shared_from_this()](boost::system::error_code ec, size_t) {
                if (ec) { self->owner_.removeSession(self->key_); return; }
                uint16_t data_len =
                    (static_cast<uint16_t>(self->chan_header_buf_[1]) << 8) |
                     static_cast<uint16_t>(self->chan_header_buf_[2]);
                uint16_t padded = data_len +
                    (data_len % 4 ? 4 - (data_len % 4) : 0);
                self->read_channel_body(data_len, padded);
            });
    }

    void read_channel_body(uint16_t data_len, uint16_t padded_len) {
        auto buf = std::make_shared<std::vector<uint8_t>>(4 + padded_len);
        (*buf)[0] = first_byte_;
        (*buf)[1] = chan_header_buf_[0];
        (*buf)[2] = chan_header_buf_[1];
        (*buf)[3] = chan_header_buf_[2];
        asio::async_read(stream_,
            asio::buffer(buf->data() + 4, padded_len),
            [self = shared_from_this(), buf, data_len](
                boost::system::error_code ec, size_t) {
                if (ec) { self->owner_.removeSession(self->key_); return; }
                if (self->on_receive_)
                    self->on_receive_(buf->data(), 4 + data_len, self->from_);
                self->read_first_byte();
            });
    }

    SslStream               stream_;
    ReceiveCallback&        on_receive_;
    TlsTransport&           owner_;
    std::string             key_;
    Endpoint                from_;
    uint8_t                 first_byte_       = 0;
    std::array<uint8_t, 19> stun_header_buf_  = {};
    std::array<uint8_t, 3>  chan_header_buf_   = {};
};

TlsTransport::TlsTransport(asio::io_context&  ioc,
                             ssl::context&      ssl_ctx,
                             const std::string& address,
                             uint16_t           port)
    : ioc_(ioc)
    , ssl_ctx_(ssl_ctx)
    , acceptor_(ioc, tcp::endpoint(asio::ip::make_address(address), port))
{}

void TlsTransport::onReceive(ReceiveCallback callback) {
    on_receive_ = std::move(callback);
}

void TlsTransport::run() {
    accept();
}

void TlsTransport::stop() {
    boost::system::error_code ec;
    acceptor_.close(ec);
}

void TlsTransport::send(const std::vector<uint8_t>& data,
                         const Endpoint&             to) {
    std::string key = to.address + ":" + std::to_string(to.port);
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(key);
    if (it != sessions_.end())
        it->second->send(data);
    else
        std::cerr << "[tls] no session for " << key << "\n";
}

void TlsTransport::accept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                auto session = std::make_shared<TlsSession>(
                    std::move(socket), ssl_ctx_, on_receive_, *this);
                {
                    std::lock_guard<std::mutex> lock(sessions_mutex_);
                    sessions_[session->key()] = session;
                }
                session->run();
            }
            accept();
        });
}

void TlsTransport::removeSession(const std::string& key) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(key);
}

}