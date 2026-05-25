#include "ws_client.hpp"

#include <iostream>

namespace signaling {

WsClient::WsClient(const std::string& host,
                    uint16_t           port,
                    bool               verify_cert)
    : host_(host)
    , port_(port)
    , verify_cert_(verify_cert)
{}

WsClient::~WsClient() {
    disconnect();
}

ssl::context WsClient::makeSslContext() {
    ssl::context ctx(ssl::context::tls_client);
    ctx.set_options(ssl::context::default_workarounds |
                    ssl::context::no_sslv2 |
                    ssl::context::no_sslv3);
    if (verify_cert_)
        ctx.set_verify_mode(ssl::verify_peer);
    else
        ctx.set_verify_mode(ssl::verify_none);
    return ctx;
}

void WsClient::connect(const std::string& token) {
    last_token_ = token;

    auto ssl_ctx = makeSslContext();
    tcp::resolver resolver(ioc_);
    auto results = resolver.resolve(host_, std::to_string(port_));

    auto ssl_stream = std::make_unique<beast::ssl_stream<tcp::socket>>(
        ioc_, ssl_ctx);

    if (!SSL_set_tlsext_host_name(ssl_stream->native_handle(),
                                   host_.c_str()))
        throw std::runtime_error("SNI setup failed");

    asio::connect(ssl_stream->next_layer(), results);
    ssl_stream->handshake(ssl::stream_base::client);

    ws_ = std::make_unique<WsStream>(std::move(*ssl_stream));

    ws_->set_option(websocket::stream_base::decorator(
        [](websocket::request_type& req) {
            req.set(boost::beast::http::field::user_agent, "chat-client/1.0");
        }));

    std::string target = "/ws?token=" + token;
    ws_->handshake(host_, target);

    connected_.store(true);
    read_thread_ = std::thread([this]() { readLoop(); });
}

void WsClient::readLoop() {
    beast::flat_buffer buf;

    while (!stop_.load()) {
        boost::system::error_code ec;
        ws_->read(buf, ec);

        if (ec) {
            connected_.store(false);
            if (!stop_.load()) {
                if (on_close_) on_close_();
                reconnect_thread_ = std::thread(
                    [this]() { reconnectLoop(last_token_); });
            }
            return;
        }

        auto msg = beast::buffers_to_string(buf.data());
        buf.consume(buf.size());

        if (on_message_) on_message_(msg);
    }
}

void WsClient::reconnectLoop(const std::string& token) {
    int delay = kBaseDelayMs;
    for (int attempt = 1; attempt <= kMaxRetries && !stop_.load(); ++attempt) {
        std::cout << "[ws] reconnect attempt " << attempt
                  << " in " << delay << "ms\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        if (stop_.load()) return;
        try {
            ioc_.restart();
            connect(token);
            std::cout << "[ws] reconnected\n";
            return;
        } catch (const std::exception& e) {
            std::cerr << "[ws] reconnect failed: " << e.what() << "\n";
        }
        delay = std::min(delay * 2, kMaxDelayMs);
    }
    std::cerr << "[ws] max retries reached, giving up\n";
}

void WsClient::send(const std::string& message) {
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (!connected_.load()) {
        std::cerr << "[ws] send failed: not connected\n";
        return;
    }
    boost::system::error_code ec;
    ws_->write(asio::buffer(message), ec);
    if (ec)
        std::cerr << "[ws] send error: " << ec.message() << "\n";
}

void WsClient::disconnect() {
    stop_.store(true);
    connected_.store(false);

    if (ws_) {
        boost::system::error_code ec;
        ws_->close(websocket::close_code::normal, ec);
    }

    if (read_thread_.joinable())
        read_thread_.join();

    ws_.reset();

    if (reconnect_thread_.joinable())
        reconnect_thread_.join();
}

}