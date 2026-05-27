#include "long_term_cred.hpp"

#include "crypto/hmac.hpp"
#include "crypto/sha256.hpp"
#include "crypto/base64.hpp"
#include "crypto/constant_time.hpp"
#include "log/logger.hpp"

#include <chrono>
#include <vector>

namespace turn_auth {

LongTermCred::LongTermCred(std::string    realm,
                            NonceManager&  nonce_manager,
                            HmacValidator& hmac_validator)
    : realm_(std::move(realm))
    , nonce_manager_(nonce_manager)
    , hmac_validator_(hmac_validator)
{}

LongTermCred::ChallengeAttrs LongTermCred::makeChallenge() {
    return ChallengeAttrs{realm_, nonce_manager_.generate()};
}

bool LongTermCred::verifyMessageIntegrity(const uint8_t*     raw_msg,
                                           size_t             raw_len,
                                           const std::string& username,
                                           const std::string& password) const {
    if (raw_len < 20) return false;

    uint16_t attrs_len = (static_cast<uint16_t>(raw_msg[2]) << 8) | raw_msg[3];
    size_t mi_pos = 0;
    bool   found  = false;
    size_t offset = 20;

    while (offset + 4 <= 20u + attrs_len && offset + 4 <= raw_len) {
        uint16_t type = (static_cast<uint16_t>(raw_msg[offset])     << 8) |
                         static_cast<uint16_t>(raw_msg[offset + 1]);
        uint16_t len  = (static_cast<uint16_t>(raw_msg[offset + 2]) << 8) |
                         static_cast<uint16_t>(raw_msg[offset + 3]);

        if (type == static_cast<uint16_t>(
                message::AttrType::MessageIntegritySha256)) {
            mi_pos = offset;
            found  = true;
            break;
        }

        offset += 4 + len;
        if (len % 4 != 0) offset += 4 - (len % 4);
    }

    if (!found) return false;
    if (mi_pos + 4 + 32 > raw_len) return false;

    uint16_t adjusted_len = static_cast<uint16_t>(mi_pos - 20 + 4 + 32);

    std::vector<uint8_t> buf(mi_pos + 4 + 32);
    std::copy(raw_msg, raw_msg + buf.size(), buf.begin());
    buf[2] = (adjusted_len >> 8) & 0xFF;
    buf[3] =  adjusted_len       & 0xFF;

    std::fill(buf.begin() + mi_pos + 4,
              buf.begin() + mi_pos + 4 + 32,
              0x00);

    auto key_bytes = crypto::long_term_key(username, realm_, password);
    std::vector<uint8_t> key_vec(key_bytes.begin(), key_bytes.end());

    std::string buf_str(buf.begin(), buf.end());
    auto computed = crypto::hmac_sha256(key_vec, buf_str);

    std::vector<uint8_t> expected(raw_msg + mi_pos + 4,
                                  raw_msg + mi_pos + 4 + 32);

    return crypto::constant_time_compare(computed, expected);
}

bool LongTermCred::verifyMessageIntegrityMd5(const uint8_t*     raw_msg,
                                              size_t             raw_len,
                                              const std::string& username,
                                              const std::string& password) const {
    if (raw_len < 20) return false;

    uint16_t attrs_len = (static_cast<uint16_t>(raw_msg[2]) << 8) | raw_msg[3];
    size_t offset = 20;
    size_t mi_pos = 0;
    bool found = false;

    while (offset + 4 <= 20u + attrs_len && offset + 4 <= raw_len) {
        uint16_t type = (static_cast<uint16_t>(raw_msg[offset])     << 8) |
                         static_cast<uint16_t>(raw_msg[offset + 1]);
        uint16_t len  = (static_cast<uint16_t>(raw_msg[offset + 2]) << 8) |
                         static_cast<uint16_t>(raw_msg[offset + 3]);

        if (type == static_cast<uint16_t>(message::AttrType::MessageIntegrity)) {
            mi_pos = offset;
            found  = true;
            break;
        }

        offset += 4 + len;
        if (len % 4 != 0) offset += 4 - (len % 4);
    }

    if (!found) return false;
    if (mi_pos + 4 + 20 > raw_len) return false;

    uint16_t adjusted_len = static_cast<uint16_t>(mi_pos - 20 + 4 + 20);

    std::vector<uint8_t> buf(raw_msg, raw_msg + mi_pos);
    buf[2] = (adjusted_len >> 8) & 0xFF;
    buf[3] =  adjusted_len       & 0xFF;

    auto key_bytes = crypto::long_term_key_md5(username, realm_, password);
    std::vector<uint8_t> key_vec(key_bytes.begin(), key_bytes.end());

    std::string buf_str(buf.begin(), buf.end());
    auto computed = crypto::hmac_sha1(key_vec, buf_str);

    std::vector<uint8_t> expected(raw_msg + mi_pos + 4,
                                  raw_msg + mi_pos + 4 + 20);

    return crypto::constant_time_compare(computed, expected);
}

AuthResult LongTermCred::authenticate(
    const message::TurnMessage& msg,
    const uint8_t*              raw_msg,
    size_t                      raw_len,
    const std::string&          allocated_username) const
{
    if (msg.msg_class == message::MessageClass::Indication)
        return AuthResult::Ok;

    auto username_attr = msg.findAttr(message::AttrType::Username);
    auto realm_attr    = msg.findAttr(message::AttrType::Realm);
    auto nonce_attr    = msg.findAttr(message::AttrType::Nonce);
    auto mi_sha256     = msg.findAttr(message::AttrType::MessageIntegritySha256);
    auto mi_md5        = msg.findAttr(message::AttrType::MessageIntegrity);

    bool has_mi = mi_sha256.has_value() || mi_md5.has_value();

    if (!username_attr || !realm_attr || !nonce_attr || !has_mi)
        return AuthResult::MissingCredentials;

    std::string username(username_attr->value.begin(),
                         username_attr->value.end());
    std::string nonce(nonce_attr->value.begin(),
                      nonce_attr->value.end());

    if (!nonce_manager_.isValid(nonce))
        return AuthResult::StaleNonce;

    {
        auto pos = username.find(':');
        if (pos == std::string::npos)
            return AuthResult::BadIntegrity;
        try {
            int64_t expiry = std::stoll(username.substr(0, pos));
            auto now = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
            if (now > expiry)
                return AuthResult::CredentialsExpired;
        } catch (...) {
            return AuthResult::BadIntegrity;
        }
    }

    std::string password = hmac_validator_.getPassword(username);
    if (password.empty())
        return AuthResult::BadIntegrity;

    if (mi_sha256.has_value()) {
        if (!verifyMessageIntegrity(raw_msg, raw_len, username, password))
            return AuthResult::BadIntegrity;
    } else if (mi_md5.has_value()) {
        if (!verifyMessageIntegrityMd5(raw_msg, raw_len, username, password))
            return AuthResult::BadIntegrity;
    }

    if (msg.method != message::Method::Allocate &&
        !allocated_username.empty() &&
        username != allocated_username)
        return AuthResult::WrongCredentials;

    return AuthResult::Ok;
}

}