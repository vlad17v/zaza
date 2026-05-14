#pragma once

#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>

namespace turn_auth {

class NonceManager {
public:
    explicit NonceManager(std::chrono::seconds ttl =
                          std::chrono::seconds(3600));

    std::string generate();
    bool        isValid(const std::string& nonce) const;
    void        invalidate(const std::string& nonce);
    void        cleanup();

private:
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    std::string generateRandom() const;

    std::chrono::seconds                       ttl_;
    std::unordered_map<std::string, TimePoint> store_;
    mutable std::mutex                         mutex_;
};

}