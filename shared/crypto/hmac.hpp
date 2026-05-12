#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>

#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

namespace crypto {

inline std::vector<uint8_t> hmac_sha1(const std::string& key,
                                       const std::string& data) {
    std::vector<uint8_t> result(EVP_MAX_MD_SIZE);
    unsigned int         len = 0;

    if (!HMAC(EVP_sha1(),
              key.data(),  key.size(),
              reinterpret_cast<const uint8_t*>(data.data()), data.size(),
              result.data(), &len)) {
        throw std::runtime_error("hmac_sha1: HMAC() failed");
    }

    result.resize(len);
    return result;
}

inline std::vector<uint8_t> hmac_sha256(const std::string& key,
                                         const std::string& data) {
    std::vector<uint8_t> result(EVP_MAX_MD_SIZE);
    unsigned int         len = 0;

    if (!HMAC(EVP_sha256(),
              key.data(),  key.size(),
              reinterpret_cast<const uint8_t*>(data.data()), data.size(),
              result.data(), &len)) {
        throw std::runtime_error("hmac_sha256: HMAC() failed");
    }

    result.resize(len);
    return result;
}

inline std::vector<uint8_t> hmac_sha256(const std::vector<uint8_t>& key,
                                         const std::string& data) {
    std::vector<uint8_t> result(EVP_MAX_MD_SIZE);
    unsigned int         len = 0;

    if (!HMAC(EVP_sha256(),
              key.data(),  key.size(),
              reinterpret_cast<const uint8_t*>(data.data()), data.size(),
              result.data(), &len)) {
        throw std::runtime_error("hmac_sha256: HMAC() failed");
    }

    result.resize(len);
    return result;
}

}