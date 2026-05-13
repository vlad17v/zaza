#include "ws_server.hpp"

#include <nlohmann/json.hpp>
#include <iostream>

namespace signaling {

WsSession::WsSession(std::unique_ptr<WsStream>               ws,
                     std::string                              userId,
                     MessageHandler                           on_message,
                     std::function<void(const std::string&)> on_close)
    : ws_(std::move(ws))
    , userId_(std::move(userId))
    , on_message_(std::move(on_message))
    , on_close_(std::move(on_close))
{}

void WsSession::run() {
    ws_->async_accept(
        [self = shared_from_this()](beast::error_code ec) {
            if (ec) {
                self->on_close_(self->userId_);
                return;
            }
            self->read();
        });
}

void WsSession::read() {
    ws_->async_read(
        buffer_,
        [self = shared_from_this()](beast::error_code ec, size_t) {
            if (ec) {
                self->on_close_(self->userId_);
                return;
            }

            auto msg = beast::buffers_to_string(self->buffer_.data());
            self->buffer_.consume(self->buffer_.size());
            self->on_message_(self->userId_, msg);
            self->read();
        });
}

void WsSession::send(const std::string& message) {
    auto msg = std::make_shared<std::string>(message);
    do_send(msg);
}

void WsSession::do_send(std::shared_ptr<std::string> msg) {
    ws_->async_write(
        asio::buffer(*msg),
        [self = shared_from_this(), msg](beast::error_code ec, size_t) {
            if (ec) self->on_close_(self->userId_);
        });
}

WsServer::WsServer(auth::AuthService& auth)
    : auth_(auth)
    , on_message_([](const std::string&, const std::string&){})
{}

void WsServer::onMessage(MessageHandler handler) {
    on_message_ = std::move(handler);
}

std::string WsServer::extractToken(const std::string& target) const {
    auto pos = target.find("token=");
    if (pos == std::string::npos) return {};
    auto end = target.find('&', pos);
    if (end == std::string::npos)
        return target.substr(pos + 6);
    return target.substr(pos + 6, end - pos - 6);
}

void WsServer::accept(std::unique_ptr<WsStream>            ws,
                      http::request<http::string_body>     req) {
    std::string token = extractToken(std::string(req.target()));
    if (token.empty()) {
        return;
    }

    auth::JwtPayload payload;
    try {
        payload = auth_.verifyToken(token);
    } catch (...) {
        return;
    }

    std::string userId = payload.userId;

    auto on_close = [this](const std::string& uid) {
        removeSession(uid);
    };

    auto session = std::make_shared<WsSession>(
        std::move(ws),
        userId,
        on_message_,
        on_close);

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_[userId] = session;
    }

    session->run();
}

bool WsServer::send(const std::string& userId,
                    const std::string& message) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(userId);
    if (it == sessions_.end()) return false;
    it->second->send(message);
    return true;
}

void WsServer::removeSession(const std::string& userId) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(userId);
}

}