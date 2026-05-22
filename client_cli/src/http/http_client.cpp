#include "http_client.hpp"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = boost::beast::http;
namespace ssl   = boost::asio::ssl;
using tcp       = asio::ip::tcp;

namespace net {

HttpClient::HttpClient(const std::string& host,
                        uint16_t           port,
                        bool               verify_cert)
    : host_(host)
    , port_(port)
    , verify_cert_(verify_cert)
{}

static ssl::context makeSslContext(bool verify_cert) {
    ssl::context ctx(ssl::context::tls_client);
    ctx.set_options(ssl::context::default_workarounds |
                    ssl::context::no_sslv2 |
                    ssl::context::no_sslv3);
    if (verify_cert)
        ctx.set_verify_mode(ssl::verify_peer);
    else
        ctx.set_verify_mode(ssl::verify_none);
    return ctx;
}

HttpResponse HttpClient::post(const std::string& path,
                               const std::string& body,
                               const std::string& content_type) {
    asio::io_context ioc;
    auto ssl_ctx = makeSslContext(verify_cert_);

    tcp::resolver resolver(ioc);
    auto results = resolver.resolve(host_, std::to_string(port_));

    beast::ssl_stream<tcp::socket> stream(ioc, ssl_ctx);
    if (!SSL_set_tlsext_host_name(stream.native_handle(), host_.c_str()))
        throw HttpError("SNI setup failed");

    asio::connect(stream.next_layer(), results);
    stream.handshake(ssl::stream_base::client);

    http::request<http::string_body> req{http::verb::post, path, 11};
    req.set(http::field::host,         host_);
    req.set(http::field::content_type, content_type);
    req.set(http::field::connection,   "close");
    req.content_length(body.size());
    req.body() = body;
    req.prepare_payload();

    http::write(stream, req);

    beast::flat_buffer buf;
    http::response<http::string_body> resp;
    http::read(stream, buf, resp);

    beast::error_code ec;
    stream.shutdown(ec);

    return HttpResponse{resp.result_int(), resp.body()};
}

HttpResponse HttpClient::get(const std::string& path,
                              const std::string& bearer_token) {
    asio::io_context ioc;
    auto ssl_ctx = makeSslContext(verify_cert_);

    tcp::resolver resolver(ioc);
    auto results = resolver.resolve(host_, std::to_string(port_));

    beast::ssl_stream<tcp::socket> stream(ioc, ssl_ctx);
    if (!SSL_set_tlsext_host_name(stream.native_handle(), host_.c_str()))
        throw HttpError("SNI setup failed");

    asio::connect(stream.next_layer(), results);
    stream.handshake(ssl::stream_base::client);

    http::request<http::empty_body> req{http::verb::get, path, 11};
    req.set(http::field::host,       host_);
    req.set(http::field::connection, "close");
    if (!bearer_token.empty())
        req.set(http::field::authorization, "Bearer " + bearer_token);
    req.prepare_payload();

    http::write(stream, req);

    beast::flat_buffer buf;
    http::response<http::string_body> resp;
    http::read(stream, buf, resp);

    beast::error_code ec;
    stream.shutdown(ec);

    return HttpResponse{resp.result_int(), resp.body()};
}

}