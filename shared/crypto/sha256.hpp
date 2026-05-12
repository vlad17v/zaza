#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>

#include <openssl/sha.h>
#include <openssl/evp.h>

namespace crypto {

inline std::vector<uint8_t> sha256(const std::string& data) {
    std::vector<uint8_t> result(SHA256_DIGEST_LENGTH);

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx)
        throw std::runtime_error("sha256: EVP_MD_CTX_new() failed");

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, data.data(), data.size()) != 1 ||
        EVP_DigestFinal_ex(ctx, result.data(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("sha256: digest failed");
    }

    EVP_MD_CTX_free(ctx);
    return result;
}

inline std::vector<uint8_t> long_term_key(const std::string& username,
                                            const std::string& realm,
                                            const std::string& password) {
    return sha256(username + ":" + realm + ":" + password);
}

}