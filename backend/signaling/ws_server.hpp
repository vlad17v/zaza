#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <memory>
#include <string>
#include <functional>
#include <unordered_map>
#include <mutex>

#include "auth/auth_service.hpp"

namespace asio      = boost::asio;
namespace beast     = boost::beast;
namespace http      = boost::beast::http;
namespace ssl       = boost::asio::ssl;
namespace websocket = boost::beast::websocket;

using tcp      = asio::ip::tcp;
using SslStream = beast::ssl_stream<tcp::socket>;
using WsStream  = websocket::stream<SslStream>;

namespace signaling {

using MessageHandler = std::function<void(const std::string& userId,
                                           const std::string& message)>;

class WsSession : public std::enable_shared_from_this<WsSession> {
public:
    WsSession(std::unique_ptr<WsStream>  ws,
              std::string                userId,
              MessageHandler             on_message,
              std::function<void(const std::string&)> on_close);

    void run(http::request<http::string_body> req);
    void send(const std::string& message);
    const std::string& userId() const { return userId_; }

private:
    void read();
    void do_send(std::shared_ptr<std::string> msg);

    std::unique_ptr<WsStream>               ws_;
    std::string                             userId_;
    MessageHandler                          on_message_;
    std::function<void(const std::string&)> on_close_;
    beast::flat_buffer                      buffer_;
    std::mutex                              send_mutex_;
};

class WsServer {
public:
    explicit WsServer(auth::AuthService& auth);

    void accept(std::unique_ptr<WsStream>                          ws,
                http::request<http::string_body>                   req);

    bool send(const std::string& userId, const std::string& message);

    void onMessage(MessageHandler handler);

private:
    std::string extractToken(const std::string& target) const;
    void        removeSession(const std::string& userId);

    auth::AuthService&                                          auth_;
    MessageHandler                                              on_message_;
    std::unordered_map<std::string, std::shared_ptr<WsSession>> sessions_;
    std::mutex                                                  sessions_mutex_;
};

}