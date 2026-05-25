#include "auth/auth_service.hpp"
#include "api/http_server.hpp"
#include "api/routes.hpp"
#include "signaling/ws_server.hpp"
#include "signaling/router.hpp"
#include "calls/call_manager.hpp"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <nlohmann/json.hpp>

#include <iostream>
#include <string>
#include <csignal>
#include <memory>
#include <thread>

namespace asio = boost::asio;
namespace ssl  = boost::asio::ssl;

static asio::io_context* g_ioc = nullptr;

static void signal_handler(int) {
    std::cout << "\n[backend] shutting down...\n";
    if (g_ioc) g_ioc->stop();
}

int main(int argc, char* argv[]) {
    const std::string host            = "0.0.0.0";
    const uint16_t    port            = 8080;
    const std::string db_path         = "../data/chat.db";
    const std::string jwt_secret      = "change_me_in_production";
    const std::string cert_file       = "../backend/certs/cert.pem";
    const std::string key_file        = "../backend/certs/key.pem";
    const std::string turn_host       = "localhost";
    const std::string turn_secret     = "turn_shared_secret";
    const int64_t     jwt_ttl         = 86400;  
    const int         expire_interval = 5;     

    std::string cert = cert_file;
    std::string key  = key_file;
    if (argc >= 3) {
        cert = argv[1];
        key  = argv[2];
    }

    std::cout << "[backend] starting on " << host << ":" << port << "\n";

    asio::io_context ioc;
    g_ioc = &ioc;

    auto work = asio::make_work_guard(ioc);

    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    ssl::context ssl_ctx(ssl::context::tls_server);
    try {
        ssl_ctx.set_options(
            ssl::context::default_workarounds |
            ssl::context::no_sslv2           |
            ssl::context::no_sslv3           |
            ssl::context::no_tlsv1           |
            ssl::context::no_tlsv1_1         |
            ssl::context::single_dh_use);
        ssl_ctx.use_certificate_chain_file(cert);
        ssl_ctx.use_private_key_file(key, ssl::context::pem);
        std::cout << "[backend] TLS loaded: " << cert << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[backend] TLS error: " << e.what() << "\n";
        std::cerr << "[backend] generate cert: openssl req -x509 "
                     "-newkey rsa:2048 -nodes "
                     "-keyout tls/key.pem -out tls/cert.pem "
                     "-days 365 -subj /CN=localhost\n";
        return 1;
    }

    auth::AuthService auth_service(jwt_secret, db_path, jwt_ttl);
    calls::CallManager call_manager(std::chrono::seconds(30));

    api::TurnConfig turn_config;
    turn_config.host          = turn_host;
    turn_config.port_plain    = 3478;
    turn_config.port_tls      = 5349;
    turn_config.shared_secret = turn_secret;
    turn_config.ttl           = 3600;

    signaling::WsServer ws_server(auth_service);

    auto rtc_config_gen = [&](const std::string& userId) {
        return api::generateRtcConfig(userId, turn_config);
    };

    signaling::Router router(ws_server, call_manager, rtc_config_gen);

    ws_server.onMessage([&router](const std::string& userId,
                                   const std::string& msg) {
        router.handle(userId, msg);
    });

    api::HttpServer http_server(ioc, ssl_ctx, host, port);
    http_server.setWsServer(ws_server);

    api::registerRoutes(http_server, auth_service, turn_config);

    asio::steady_timer expire_timer(ioc);
    std::function<void()> schedule_expire = [&]() {
        expire_timer.expires_after(
            std::chrono::seconds(expire_interval));
        expire_timer.async_wait(
            [&](boost::system::error_code ec) {
                if (!ec) {
                    router.checkExpired();
                    schedule_expire();
                }
            });
    };
    schedule_expire();

    http_server.run();
    std::cout << "[backend] listening on https://"
              << host << ":" << port << "\n";

    unsigned int threads = std::max(2u,
        std::thread::hardware_concurrency());
    std::cout << "[backend] io threads: " << threads << "\n";

    std::vector<std::thread> thread_pool;
    for (unsigned i = 1; i < threads; ++i)
        thread_pool.emplace_back([&ioc]() { ioc.run(); });

    ioc.run();

    work.reset();

    for (auto& t : thread_pool)
        if (t.joinable()) t.join();

    std::cout << "[backend] stopped\n";
    return 0;
}