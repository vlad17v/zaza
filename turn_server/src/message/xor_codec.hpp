#pragma once
#include <cstdint>

namespace message {

static constexpr uint32_t kMagicCookie = 0x2112A442;

inline uint16_t xor_port(uint16_t port) {
    return port ^ static_cast<uint16_t>(kMagicCookie >> 16);
}

inline uint32_t xor_addr_v4(uint32_t addr) {
    return addr ^ kMagicCookie;
}

}