#include "jwt.hpp"

#include <jwt-cpp/jwt.h>
#include <chrono>

namespace auth {

Jwt::Jwt(std::string secret)
    : secret_(std::move(secret))
{}

std::string Jwt::sign(const std::string& userId, int64_t ttl_seconds) const {
    auto now = std::chrono::system_clock::now();
    auto exp = now + std::chrono::seconds(ttl_seconds);

    return jwt::create()
        .set_type("JWT")
        .set_issued_at(now)
        .set_expires_at(exp)
        .set_payload_claim("userId", jwt::claim(userId))
        .sign(jwt::algorithm::hs256{ secret_ });
}

JwtPayload Jwt::verify(const std::string& token) const {
    try {
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{ secret_ })
            .with_type("JWT");

        auto decoded = jwt::decode(token);
        verifier.verify(decoded);

        JwtPayload payload;
        payload.userId = decoded.get_payload_claim("userId").as_string();
        payload.exp    = std::chrono::system_clock::to_time_t(
                             decoded.get_expires_at());
        return payload;

    } catch (const jwt::error::token_verification_exception& e) {
        std::string msg = e.what();
        if (msg.find("expired") != std::string::npos)
            throw JwtExpired(msg);
        throw JwtInvalid(msg);

    } catch (const std::exception& e) {
        throw JwtInvalid(e.what());
    }
}

}