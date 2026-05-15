#include "call_manager.hpp"

#include <stdexcept>
#include <random>
#include <sstream>
#include <iomanip>

namespace calls {

CallManager::CallManager(std::chrono::seconds ring_timeout)
    : ring_timeout_(ring_timeout)
{}

std::string CallManager::generateUuid() {
    static std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<uint64_t> dist;

    uint64_t a = dist(rng);
    uint64_t b = dist(rng);

    a = (a & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    b = (b & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(8)  << ((a >> 32) & 0xFFFFFFFF) << '-'
        << std::setw(4)  << ((a >> 16) & 0xFFFF)     << '-'
        << std::setw(4)  << ( a        & 0xFFFF)      << '-'
        << std::setw(4)  << ((b >> 48) & 0xFFFF)      << '-'
        << std::setw(12) << ( b        & 0xFFFFFFFFFFFFULL);
    return oss.str();
}

std::string CallManager::create(const std::string& callerId,
                                 const std::string& calleeId) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (isBusy(callerId))
        throw std::runtime_error("caller is busy");
    if (isBusy(calleeId))
        throw std::runtime_error("callee is busy");

    CallSession s;
    s.callId    = generateUuid();
    s.callerId  = callerId;
    s.calleeId  = calleeId;
    s.state     = CallState::Ringing;
    s.createdAt = std::chrono::steady_clock::now();

    sessions_[s.callId] = s;
    return s.callId;
}

CallSession CallManager::accept(const std::string& callId,
                                 const std::string& calleeId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(callId);
    if (it == sessions_.end())
        throw std::runtime_error("call not found: " + callId);
    if (it->second.calleeId != calleeId)
        throw std::runtime_error("wrong callee");
    if (it->second.state != CallState::Ringing)
        throw std::runtime_error("call not ringing");

    it->second.state = CallState::Active;
    return it->second;
}

CallSession CallManager::reject(const std::string& callId,
                                 const std::string& calleeId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(callId);
    if (it == sessions_.end())
        throw std::runtime_error("call not found: " + callId);
    if (it->second.calleeId != calleeId)
        throw std::runtime_error("wrong callee");
    if (it->second.state != CallState::Ringing)
        throw std::runtime_error("call not ringing");

    it->second.state = CallState::Ended;
    auto s = it->second;
    sessions_.erase(it);
    return s;
}

CallSession CallManager::hangup(const std::string& callId,
                                 const std::string& userId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(callId);
    if (it == sessions_.end())
        throw std::runtime_error("call not found: " + callId);

    auto& s = it->second;
    if (s.callerId != userId && s.calleeId != userId)
        throw std::runtime_error("user not in call");

    s.state = CallState::Ended;
    auto copy = s;
    sessions_.erase(it);
    return copy;
}

std::optional<CallSession> CallManager::find(
    const std::string& callId) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(callId);
    if (it == sessions_.end()) return std::nullopt;
    return it->second;
}

std::vector<std::string> CallManager::expireRinging() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    std::vector<std::string> expired;

    for (auto it = sessions_.begin(); it != sessions_.end(); ) {
        auto& s = it->second;
        if (s.state == CallState::Ringing &&
            now - s.createdAt > ring_timeout_) {
            expired.push_back(s.callId);
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
    return expired;
}

bool CallManager::isBusy(const std::string& userId) const {
    for (auto& [id, s] : sessions_) {
        if (s.state != CallState::Ended &&
            (s.callerId == userId || s.calleeId == userId))
            return true;
    }
    return false;
}

}