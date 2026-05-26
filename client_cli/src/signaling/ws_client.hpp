#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <chrono>

namespace asio      = boost::asio;
namespace beast     = boost::beast;
namespace ssl       = boost::asio::ssl;
namespace websocket = boost::beast::websocket;
using tcp           = asio::ip::tcp;

namespace signaling {

using MessageCallback = std::function<void(const std::string& message)>;
using CloseCallback   = std::function<void()>;

class WsClient {
public:
    WsClient(const std::string& host,
             uint16_t           port,
             bool               verify_cert = false);
    ~WsClient();

    void connect(const std::string& token);

    void send(const std::string& message);

    void disconnect();

    bool isConnected() const { return connected_.load(); }

    void onMessage(MessageCallback cb) { on_message_ = std::move(cb); }
    void onClose(CloseCallback cb)     { on_close_   = std::move(cb); }

private:
    using SslStream = beast::ssl_stream<tcp::socket>;
    using WsStream  = websocket::stream<SslStream>;

    void readLoop();
    void reconnectLoop(const std::string& token);
    ssl::context makeSslContext();

    std::string host_;
    uint16_t    port_;
    bool        verify_cert_;

    asio::io_context        ioc_;
    std::unique_ptr<WsStream> ws_;

    std::atomic<bool>       connected_{false};
    std::atomic<bool>       stop_{false};

    std::thread             read_thread_;
    std::thread             reconnect_thread_;

    std::mutex              send_mutex_;

    MessageCallback         on_message_;
    CloseCallback           on_close_;

    std::string             last_token_;

    ssl::context            ssl_ctx_; 

    static constexpr int kMaxRetries    = 10;
    static constexpr int kBaseDelayMs   = 500;
    static constexpr int kMaxDelayMs    = 30000;
};

}