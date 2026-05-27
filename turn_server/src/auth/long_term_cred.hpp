#pragma once

#include "nonce_manager.hpp"
#include "hmac_validator.hpp"
#include "message/parser.hpp"

#include <string>
#include <vector>
#include <cstdint>

namespace turn_auth {

enum class AuthResult {
    Ok,
    MissingCredentials,
    StaleNonce,
    BadIntegrity,
    WrongCredentials,
    CredentialsExpired,
};

class LongTermCred {
public:
    LongTermCred(std::string    realm,
                 NonceManager&  nonce_manager,
                 HmacValidator& hmac_validator);

    AuthResult authenticate(const message::TurnMessage& msg,
                            const uint8_t*              raw_msg,
                            size_t                      raw_len,
                            const std::string&          allocated_username) const;

    struct ChallengeAttrs {
        std::string realm;
        std::string nonce;
    };
    ChallengeAttrs makeChallenge();

    const std::string& realm() const { return realm_; }

private:
    bool verifyMessageIntegrity(const uint8_t*     raw_msg,
                                size_t             raw_len,
                                const std::string& username,
                                const std::string& password) const;

    bool verifyMessageIntegrityMd5(const uint8_t*     raw_msg,
                                size_t             raw_len,
                                const std::string& username,
                                const std::string& password) const;

    std::string    realm_;
    NonceManager&  nonce_manager_;
    HmacValidator& hmac_validator_;
};

}