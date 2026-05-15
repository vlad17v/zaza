#pragma once

#include "http_server.hpp"
#include "auth/auth_service.hpp"
#include "crypto/hmac.hpp"
#include "crypto/base64.hpp"

#include <string>
#include <chrono>

namespace api {

struct TurnConfig {
    std::string host;
    uint16_t    port_plain = 3478;
    uint16_t    port_tls   = 5349;
    std::string shared_secret;
    int64_t     ttl        = 3600;
};

void registerRoutes(HttpServer&         server,
                    auth::AuthService&  auth_service,
                    const TurnConfig&   turn_config);

std::string generateRtcConfig(const std::string& userId,
                               const TurnConfig&  turn_config);

}