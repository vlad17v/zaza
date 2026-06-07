#include "crypto/hmac.hpp"
#include "crypto/sha256.hpp"
#include "crypto/base64.hpp"
#include "crypto/constant_time.hpp"
#include "config/config.hpp"
#include <iostream>
#include <cassert>
#include <iomanip>
#include <sstream>
#include <stdexcept>

static std::string to_hex(const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    for (uint8_t b : data)
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    return oss.str();
}

int main() {
    {
        auto h1 = crypto::hmac_sha1("secret", "1715510400:user1");
        std::string cred = crypto::base64_encode(h1);
        assert(!cred.empty());
        assert(cred == "EFrMV7wNTxCcOVqfLmBrjYLpc2c=");
        assert(h1.size() == 20);

        auto h1_empty_key = crypto::hmac_sha1("", "data");
        assert(h1_empty_key.size() == 20);

        auto h1_empty_data = crypto::hmac_sha1("key", "");
        assert(h1_empty_data.size() == 20);

        auto h1_both_empty = crypto::hmac_sha1("", "");
        assert(h1_both_empty.size() == 20);

        assert(h1_empty_key != h1_empty_data);
    }
    std::cout << "[hmac_sha1] OK\n";

    {
        auto h256 = crypto::hmac_sha256(
            "key",
            "The quick brown fox jumps over the lazy dog"
        );
        assert(h256.size() == 32);
        assert(to_hex(h256) ==
            "f7bc83f430538424b13298e6aa6fb143"
            "ef4d59a14946175997479dbc2d1a3cd8");

        auto h256_empty_key = crypto::hmac_sha256("", "data");
        assert(h256_empty_key.size() == 32);

        auto h256_empty_data = crypto::hmac_sha256("key", "");
        assert(h256_empty_data.size() == 32);

        auto h256_both_empty = crypto::hmac_sha256("", "");
        assert(h256_both_empty.size() == 32);

        assert(h256_empty_key != h256_empty_data);

        std::vector<uint8_t> vec_key = {'k', 'e', 'y'};
        auto h256_vec = crypto::hmac_sha256(vec_key, "The quick brown fox jumps over the lazy dog");
        assert(h256_vec == h256);
    }
    std::cout << "[hmac_sha256] OK\n";

    {
        auto key = crypto::long_term_key("user1", "chat.example.com", "password123");
        assert(key.size() == 32);
        assert(to_hex(key) ==
            "1875dd9e5e407806aa7cd2339e9eb217"
            "d26a42e17f0f16a02dd32b7033327b9f");

        auto key_empty = crypto::long_term_key("", "", "");
        assert(key_empty.size() == 32);

        auto key_same_concat = crypto::long_term_key("a:b", "", "c");
        auto key_diff_split  = crypto::long_term_key("a", "b", "c");
        assert(key_same_concat != key_diff_split);
    }
    std::cout << "[sha256/long_term_key] OK\n";

    {
        std::vector<uint8_t> a = {1, 2, 3};
        std::vector<uint8_t> b = {1, 2, 3};
        std::vector<uint8_t> c = {1, 2, 4};
        assert( crypto::constant_time_compare(a, b));
        assert(!crypto::constant_time_compare(a, c));

        std::vector<uint8_t> shorter = {1, 2};
        assert(!crypto::constant_time_compare(a, shorter));
        assert(!crypto::constant_time_compare(shorter, a));

        std::vector<uint8_t> empty1, empty2;
        assert( crypto::constant_time_compare(empty1, empty2));
        assert(!crypto::constant_time_compare(empty1, a));

        assert( crypto::constant_time_compare(std::string("abc"), std::string("abc")));
        assert(!crypto::constant_time_compare(std::string("abc"), std::string("abd")));
        assert(!crypto::constant_time_compare(std::string("ab"),  std::string("abc")));
        assert( crypto::constant_time_compare(std::string(""),    std::string("")));

        uint8_t raw_a[] = {1, 2, 3};
        uint8_t raw_b[] = {1, 2, 3};
        uint8_t raw_c[] = {1, 2, 4};
        assert( crypto::constant_time_compare(raw_a, raw_b, 3));
        assert(!crypto::constant_time_compare(raw_a, raw_c, 3));
    }
    std::cout << "[constant_time_compare] OK\n";

    {
        std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
        auto encoded = crypto::base64_encode(data);
        assert(encoded == "3q2+7w==");
        auto decoded = crypto::base64_decode(encoded);
        assert(decoded == data);

        std::vector<uint8_t> one_byte = {0x41};
        auto enc1 = crypto::base64_encode(one_byte);
        assert(enc1 == "QQ==");
        assert(crypto::base64_decode(enc1) == one_byte);

        std::vector<uint8_t> two_bytes = {0x41, 0x42};
        auto enc2 = crypto::base64_encode(two_bytes);
        assert(enc2 == "QUI=");
        assert(crypto::base64_decode(enc2) == two_bytes);

        std::vector<uint8_t> three_bytes = {0x41, 0x42, 0x43};
        auto enc3 = crypto::base64_encode(three_bytes);
        assert(enc3 == "QUJD");
        assert(crypto::base64_decode(enc3) == three_bytes);

        auto empty_b64 = crypto::base64_encode({});
        assert(empty_b64.empty());
        auto empty_decoded = crypto::base64_decode("");
        assert(empty_decoded.empty());

        try {
            crypto::base64_decode("not_valid!!!");
            assert(false);
        } catch (const std::invalid_argument&) {}

        try {
            crypto::base64_decode("abc");
            assert(false);
        } catch (const std::invalid_argument&) {}
    }
    std::cout << "[base64] OK\n";

    std::cout << "\n=== Config ===\n";

    {
        const char* tmp = "/tmp/test_config.env";
        {
            std::ofstream f(tmp);
            f << "# comment\n";
            f << "KEY1=value1\n";
            f << "KEY2=42\n";
            f << "KEY3=\n";
            f << "KEY4=\"quoted\"\n";
        }
        Config::instance().load(tmp);
        assert(Config::instance().get("KEY1") == "value1");
        assert(Config::instance().getInt("KEY2", 0) == 42);
        assert(Config::instance().get("KEY3") == "");
        assert(Config::instance().get("KEY4") == "quoted");
        assert(Config::instance().get("MISSING", "default") == "default");
        assert(Config::instance().getInt("MISSING", 99) == 99);
        std::cout << "[config] load and get OK\n";
        std::remove(tmp);
    }

    std::cout << "\nAll tests passed\n";
    return 0;
}