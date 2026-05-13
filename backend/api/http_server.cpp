#include "http_server.hpp"
#include "signaling/ws_server.hpp"

#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <nlohmann/json.hpp>
#include <iostream>

namespace api {

namespace {

using SslStream   = beast::ssl_stream<tcp::socket>;
using WsStream    = beast::websocket::stream<SslStream>;
using HttpRequest = http::request<http::string_body>;
using HttpResp    = http::response<http::string_body>;

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket                                     socket,
            ssl::context&                                   ssl_ctx,
            const std::unordered_map<std::string, Handler>& routes,
            signaling::WsServer&                            ws_server)
        : stream_(std::move(socket), ssl_ctx)
        , routes_(routes)
        , ws_server_(ws_server)
    {}

    void run() {
        stream_.async_handshake(
            ssl::stream_base::server,
            [self = shared_from_this()](beast::error_code ec) {
                if (ec) return;
                self->read_request();
            });
    }

private:
    void read_request() {
        auto req = std::make_shared<HttpRequest>();
        http::async_read(
            stream_, buffer_, *req,
            [self = shared_from_this(), req](beast::error_code ec, size_t) {
                if (ec) return;
                self->handle_request(req);
            });
    }

    void handle_request(std::shared_ptr<HttpRequest> req) {
        if (beast::websocket::is_upgrade(*req)) {
            auto ws = std::make_unique<WsStream>(std::move(stream_));
            ws_server_.accept(std::move(ws), std::move(*req));
            return;
        }

        auto path = std::string(req->target());
        if (auto pos = path.find('?'); pos != std::string::npos)
            path = path.substr(0, pos);

        std::string route_key = std::string(req->method_string()) + " " + path;
        auto it = routes_.find(route_key);

        HttpResp res;
        res.version(req->version());
        res.keep_alive(false);
        res.set(http::field::content_type, "application/json");

        if (it != routes_.end()) {
            Request r;
            r.method = std::string(req->method_string());
            r.target = std::string(req->target());
            r.body   = req->body();
            for (auto& field : *req)
                r.headers[std::string(field.name_string())] =
                    std::string(field.value());

            try {
                auto resp = it->second(r);
                res.result(resp.status);
                res.body() = resp.body;
                res.set(http::field::content_type, resp.content_type);
            } catch (const std::exception& e) {
                res.result(http::status::internal_server_error);
                res.body() = std::string(R"({"error":")") + e.what() + "\"}";
            }
        } else {
            res.result(http::status::not_found);
            res.body() = R"({"error":"not found"})";
        }

        res.prepare_payload();
        auto res_ptr = std::make_shared<HttpResp>(std::move(res));
        http::async_write(
            stream_, *res_ptr,
            [self = shared_from_this(), res_ptr](beast::error_code, size_t) {
                beast::error_code ec;
                self->stream_.shutdown(ec);
            });
    }

    SslStream                                        stream_;
    beast::flat_buffer                               buffer_;
    const std::unordered_map<std::string, Handler>&  routes_;
    signaling::WsServer&                             ws_server_;
};

}

HttpServer::HttpServer(asio::io_context&  ioc,
                       ssl::context&      ssl_ctx,
                       const std::string& address,
                       uint16_t           port)
    : ioc_(ioc)
    , ssl_ctx_(ssl_ctx)
    , acceptor_(ioc, tcp::endpoint(asio::ip::make_address(address), port))
    , ws_server_(nullptr)
{}

void HttpServer::route(const std::string& method,
                       const std::string& path,
                       Handler            handler) {
    routes_[method + " " + path] = std::move(handler);
}

void HttpServer::setWsServer(signaling::WsServer& ws_server) {
    ws_server_ = &ws_server;
}

void HttpServer::run() {
    accept();
}

void HttpServer::accept() {
    acceptor_.async_accept(
        [this](beast::error_code ec, tcp::socket socket) {
            if (!ec && ws_server_) {
                std::make_shared<Session>(
                    std::move(socket),
                    ssl_ctx_,
                    routes_,
                    *ws_server_)->run();
            }
            accept();
        });
}

}