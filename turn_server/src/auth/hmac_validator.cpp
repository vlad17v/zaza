#include "hmac_validator.hpp"

#include "crypto/hmac.hpp"
#include "crypto/base64.hpp"
#include "crypto/constant_time.hpp"

#include <chrono>

namespace turn_auth {

HmacValidator::HmacValidator(std::string shared_secret)
    : shared_secret_(std::move(shared_secret))
{}

bool HmacValidator::parseExpiry(const std::string& username,
                                 int64_t&           expiry) const {
    auto pos = username.find(':');
    if (pos == std::string::npos) return false;
    try {
        expiry = std::stoll(username.substr(0, pos));
    } catch (...) {
        return false;
    }
    return true;
}

std::string HmacValidator::computeHmac(const std::string& username) const {
    auto raw = crypto::hmac_sha1(shared_secret_, username);
    return crypto::base64_encode(raw);
}

std::string HmacValidator::getPassword(const std::string& username) const {
    if (username.find(':') == std::string::npos) return {};
    return computeHmac(username);
}

bool HmacValidator::validate(const std::string& username,
                              const std::string& password) const {
    int64_t expiry = 0;
    if (!parseExpiry(username, expiry)) return false;

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();
    if (now > expiry) return false;

    auto expected = computeHmac(username);
    return crypto::constant_time_compare(expected, password);
}

HmacCredentials HmacValidator::generate(const std::string& user_id,
                                         int64_t            ttl_seconds) const {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();
    HmacCredentials creds;
    creds.username = std::to_string(now + ttl_seconds) + ":" + user_id;
    creds.password = computeHmac(creds.username);
    return creds;
}

}