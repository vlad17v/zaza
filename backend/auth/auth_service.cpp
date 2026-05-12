#include "auth_service.hpp"

namespace auth {

AuthService::AuthService(std::string jwt_secret,
                         std::string db_path,
                         int64_t     token_ttl_seconds)
    : jwt_(std::move(jwt_secret))
    , store_(std::move(db_path))
    , ttl_(token_ttl_seconds)
{}

LoginResult AuthService::login(const std::string& userId,
                                const std::string& password) const {
    if (!store_.checkPassword(userId, password))
        throw LoginError("invalid userId or password");

    LoginResult result;
    result.userId = userId;
    result.token  = jwt_.sign(userId, ttl_);
    return result;
}

JwtPayload AuthService::verifyToken(const std::string& token) const {
    return jwt_.verify(token);
}

void AuthService::registerUser(const std::string& userId,
                                const std::string& password) {
    store_.addUser(userId, password);
}

}