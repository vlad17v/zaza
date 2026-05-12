#include "crypto/hmac.hpp"
#include "crypto/sha256.hpp"
#include "crypto/base64.hpp"
#include "crypto/constant_time.hpp"

#include <iostream>
#include <cassert>
#include <iomanip>
#include <sstream>

static std::string to_hex(const std::vector<uint8_t>& data) {
    std::ostringstream oss;

    for (uint8_t b : data) {
        oss << std::hex
            << std::setw(2)
            << std::setfill('0')
            << static_cast<int>(b);
    }

    return oss.str();
}

int main() {
    auto h1 = crypto::hmac_sha1("secret", "1715510400:user1");
    std::string cred = crypto::base64_encode(h1);

    assert(!cred.empty());
    assert(cred == "EFrMV7wNTxCcOVqfLmBrjYLpc2c=");

    auto h256 = crypto::hmac_sha256(
        "key",
        "The quick brown fox jumps over the lazy dog"
    );

    assert(h256.size() == 32);

    auto h256_hex = to_hex(h256);

    assert(
        h256_hex ==
        "f7bc83f430538424b13298e6aa6fb143"
        "ef4d59a14946175997479dbc2d1a3cd8"
    );

    auto key = crypto::long_term_key(
        "user1",
        "chat.example.com",
        "password123"
    );

    assert(key.size() == 32);

    auto key_hex = to_hex(key);

    assert(
        key_hex ==
        "1875dd9e5e407806aa7cd2339e9eb217"
        "d26a42e17f0f16a02dd32b7033327b9f"
    );

    std::vector<uint8_t> a = {1, 2, 3};
    std::vector<uint8_t> b = {1, 2, 3};
    std::vector<uint8_t> c = {1, 2, 4};

    assert(crypto::constant_time_compare(a, b));
    assert(!crypto::constant_time_compare(a, c));

    std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};

    auto encoded = crypto::base64_encode(data);

    assert(encoded == "3q2+7w==");

    auto decoded = crypto::base64_decode(encoded);

    assert(decoded == data);

    auto empty_hmac = crypto::hmac_sha256("", "");

    assert(empty_hmac.size() == 32);

    auto empty_b64 = crypto::base64_encode({});

    assert(empty_b64.empty());

    auto empty_decoded = crypto::base64_decode("");

    assert(empty_decoded.empty());

    std::cout << "All tests passed\n";

    return 0;
}