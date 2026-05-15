#include "nonce_manager.hpp"

#include <openssl/rand.h>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace turn_auth {

NonceManager::NonceManager(std::chrono::seconds ttl)
    : ttl_(ttl)
{}

std::string NonceManager::generateRandom() const {
    uint8_t buf[16];
    if (RAND_bytes(buf, sizeof(buf)) != 1)
        throw std::runtime_error("RAND_bytes failed");
    std::ostringstream oss;
    for (auto b : buf)
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(b);
    return oss.str();
}

std::string NonceManager::generate() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string nonce;
    do {
        nonce = generateRandom();
    } while (store_.count(nonce));
    store_[nonce] = Clock::now() + ttl_;
    return nonce;
}

bool NonceManager::isValid(const std::string& nonce) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = store_.find(nonce);
    if (it == store_.end()) return false;
    return Clock::now() < it->second;
}

void NonceManager::invalidate(const std::string& nonce) {
    std::lock_guard<std::mutex> lock(mutex_);
    store_.erase(nonce);
}

void NonceManager::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = Clock::now();
    for (auto it = store_.begin(); it != store_.end(); ) {
        if (now >= it->second)
            it = store_.erase(it);
        else
            ++it;
    }
}

}