#pragma once

#include <string>
#include <cstdint>

namespace turn_auth {

struct HmacCredentials {
    std::string username;
    std::string password;
};

class HmacValidator {
public:
    explicit HmacValidator(std::string shared_secret);

    bool        validate(const std::string& username,
                         const std::string& password) const;

    std::string getPassword(const std::string& username) const;

    HmacCredentials generate(const std::string& user_id,
                              int64_t            ttl_seconds) const;

private:
    bool        parseExpiry(const std::string& username,
                            int64_t&           expiry) const;
    std::string computeHmac(const std::string& username) const;

    std::string shared_secret_;
};

}