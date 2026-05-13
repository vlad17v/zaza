#pragma once

#include <string>
#include <cstdint>
#include <stdexcept>

namespace auth {

struct JwtPayload {
    std::string userId;
    int64_t     exp;
};

struct JwtExpired    : std::runtime_error { using std::runtime_error::runtime_error; };
struct JwtInvalid    : std::runtime_error { using std::runtime_error::runtime_error; };

class Jwt {
public:
    explicit Jwt(std::string secret);

    std::string     sign(const std::string& userId, int64_t ttl_seconds) const;

    JwtPayload      verify(const std::string& token) const;

private:
    std::string secret_;
};

}