#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

namespace crypto {

inline std::string base64_encode(const std::vector<uint8_t>& data) {
    BIO* b64  = BIO_new(BIO_f_base64());
    BIO* bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);

    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, data.data(), static_cast<int>(data.size()));
    BIO_flush(b64);

    BUF_MEM* bptr = nullptr;
    BIO_get_mem_ptr(b64, &bptr);

    std::string result(bptr->data, bptr->length);
    BIO_free_all(b64);
    return result;
}

inline std::vector<uint8_t> base64_decode(const std::string& input) {
    BIO* b64  = BIO_new(BIO_f_base64());
    BIO* bmem = BIO_new_mem_buf(input.data(), static_cast<int>(input.size()));
    b64 = BIO_push(b64, bmem);

    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    std::vector<uint8_t> result(input.size());
    int len = BIO_read(b64, result.data(), static_cast<int>(result.size()));
    BIO_free_all(b64);

    if (len < 0)
        throw std::invalid_argument("base64_decode: invalid input");

    result.resize(static_cast<size_t>(len));
    return result;
}

}