#include "crypto/hmac.hpp"
#include "crypto/sha256.hpp"
#include "crypto/base64.hpp"
#include "crypto/constant_time.hpp"
#include <iostream>
#include <cassert>

int main() {
    auto h1 = crypto::hmac_sha1("secret", "1715510400:user1");
    std::string cred = crypto::base64_encode(h1);
    std::cout << "TURN credential: " << cred << "\n";
    assert(!cred.empty());

    auto h256 = crypto::hmac_sha256("key", "data");
    assert(h256.size() == 32);

    auto key = crypto::long_term_key("user1", "chat.example.com", "password123");
    assert(key.size() == 32);

    std::vector<uint8_t> a = {1, 2, 3};
    std::vector<uint8_t> b = {1, 2, 3};
    std::vector<uint8_t> c = {1, 2, 4};
    assert( crypto::constant_time_compare(a, b));
    assert(!crypto::constant_time_compare(a, c));

    std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
    auto encoded = crypto::base64_encode(data);
    auto decoded = crypto::base64_decode(encoded);
    assert(decoded == data);

    std::cout << "All tests passed\n";
    return 0;
}