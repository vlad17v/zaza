#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>

#include <memory>
#include <string>
#include <functional>
#include <unordered_map>

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = boost::beast::http;
namespace ssl   = boost::asio::ssl;

using tcp = asio::ip::tcp;

namespace signaling { class WsServer; }

namespace api {

struct Request {
    std::string                              method;
    std::string                              target;
    std::string                              body;
    std::unordered_map<std::string, std::string> headers;
};

struct Response {
    unsigned    status       = 200;
    std::string body;
    std::string content_type = "application/json";
};

using Handler = std::function<Response(const Request&)>;

class HttpServer {
public:
    HttpServer(asio::io_context&     ioc,
               ssl::context&         ssl_ctx,
               const std::string&    address,
               uint16_t              port);

    void route(const std::string& method,
               const std::string& path,
               Handler            handler);

    void setWsServer(signaling::WsServer& ws_server);
    void run();

private:
    void accept();

    asio::io_context&                            ioc_;
    ssl::context&                                ssl_ctx_;
    tcp::acceptor                                acceptor_;
    std::unordered_map<std::string, Handler>     routes_;
    signaling::WsServer*                         ws_server_;
};

}