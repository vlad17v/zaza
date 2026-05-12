#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

#include <openssl/crypto.h>

namespace crypto {

inline bool constant_time_compare(const uint8_t* a,
                                   const uint8_t* b,
                                   size_t         len) {
    return CRYPTO_memcmp(a, b, len) == 0;
}

inline bool constant_time_compare(const std::vector<uint8_t>& a,
                                   const std::vector<uint8_t>& b) {
    if (a.size() != b.size()) return false;
    return constant_time_compare(a.data(), b.data(), a.size());
}

inline bool constant_time_compare(const std::string& a,
                                   const std::string& b) {
    if (a.size() != b.size()) return false;
    return constant_time_compare(
        reinterpret_cast<const uint8_t*>(a.data()),
        reinterpret_cast<const uint8_t*>(b.data()),
        a.size()
    );
}

}