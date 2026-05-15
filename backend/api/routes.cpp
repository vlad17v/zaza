#include "routes.hpp"

#include <nlohmann/json.hpp>
#include <chrono>
#include <stdexcept>

namespace api {

using json = nlohmann::json;

static std::string extractBearer(const Request& req) {
    auto it = req.headers.find("Authorization");
    if (it == req.headers.end()) {
        it = req.headers.find("authorization");
        if (it == req.headers.end()) return {};
    }
    const auto& val = it->second;
    if (val.rfind("Bearer ", 0) != 0) return {};
    return val.substr(7);
}

std::string generateRtcConfig(const std::string& userId,
                               const TurnConfig&  cfg) {
    auto expiry   = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count() + cfg.ttl;
    auto username = std::to_string(expiry) + ":" + userId;
    auto raw_hmac = crypto::hmac_sha1(cfg.shared_secret, username);
    auto password = crypto::base64_encode(raw_hmac);

    json ice_server = {
        {"urls", {
            "turn:"  + cfg.host + ":" + std::to_string(cfg.port_plain),
            "turns:" + cfg.host + ":" + std::to_string(cfg.port_tls)
        }},
        {"username",   username},
        {"credential", password}
    };

    json config = {{"iceServers", json::array({ice_server})}};
    return config.dump();
}

void registerRoutes(HttpServer&        server,
                    auth::AuthService& auth_service,
                    const TurnConfig&  turn_config) {

    server.route("POST", "/api/auth/login",
        [&auth_service](const Request& req) -> Response {
            json body;
            try {
                body = json::parse(req.body);
            } catch (...) {
                return Response{400, R"({"error":"invalid json"})"};
            }

            if (!body.contains("userId") || !body.contains("password"))
                return Response{400, R"({"error":"missing userId or password"})"};

            std::string userId   = body["userId"];
            std::string password = body["password"];

            try {
                auto result = auth_service.login(userId, password);
                json resp = {
                    {"token",  result.token},
                    {"userId", result.userId}
                };
                return Response{200, resp.dump()};
            } catch (const auth::LoginError&) {
                return Response{401, R"({"error":"invalid credentials"})"};
            }
        });

    server.route("GET", "/api/turn/credentials",
        [&auth_service, turn_config](const Request& req) -> Response {
            auto token = extractBearer(req);
            if (token.empty())
                return Response{401, R"({"error":"missing token"})"};

            auth::JwtPayload payload;
            try {
                payload = auth_service.verifyToken(token);
            } catch (const auth::JwtExpired&) {
                return Response{401, R"({"error":"token expired"})"};
            } catch (const auth::JwtInvalid&) {
                return Response{401, R"({"error":"invalid token"})"};
            }

            auto expiry   = std::chrono::duration_cast<std::chrono::seconds>(
                                std::chrono::system_clock::now()
                                    .time_since_epoch())
                                .count() + turn_config.ttl;
            auto username = std::to_string(expiry) + ":" + payload.userId;
            auto raw_hmac = crypto::hmac_sha1(turn_config.shared_secret,
                                               username);
            auto password = crypto::base64_encode(raw_hmac);

            json resp = {
                {"urls", {
                    "turn:"  + turn_config.host + ":" +
                        std::to_string(turn_config.port_plain),
                    "turns:" + turn_config.host + ":" +
                        std::to_string(turn_config.port_tls)
                }},
                {"username",   username},
                {"credential", password},
                {"ttl",        turn_config.ttl}
            };
            return Response{200, resp.dump()};
        });

    server.route("POST", "/api/auth/register",
        [&auth_service](const Request& req) -> Response {
            json body;
            try {
                body = json::parse(req.body);
            } catch (...) {
                return Response{400, R"({"error":"invalid json"})"};
            }

            if (!body.contains("userId") || !body.contains("password"))
                return Response{400, R"({"error":"missing fields"})"};

            try {
                auth_service.registerUser(body["userId"], body["password"]);
                return Response{200, R"({"ok":true})"};
            } catch (const auth::UserStoreError& e) {
                return Response{409, std::string(R"({"error":")") +
                                     e.what() + "\"}"};
            }
        });
}

}