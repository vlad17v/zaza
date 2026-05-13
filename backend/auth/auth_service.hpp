#pragma once

#include "jwt.hpp"
#include "user_store.hpp"
#include <string>

namespace auth {

struct LoginResult {
    std::string token;
    std::string userId;
};

struct LoginError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class AuthService {
public:
    AuthService(std::string jwt_secret,
                std::string db_path,
                int64_t     token_ttl_seconds = 86400);

    LoginResult login(const std::string& userId,
                      const std::string& password) const;

    JwtPayload  verifyToken(const std::string& token) const;

    void        registerUser(const std::string& userId,
                             const std::string& password);

private:
    Jwt         jwt_;
    UserStore   store_;
    int64_t     ttl_;
};

}