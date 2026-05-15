#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <mutex>
#include <optional>

namespace calls {

enum class CallState {
    Ringing,
    Active,
    Ended,
};

struct CallSession {
    std::string callId;
    std::string callerId;
    std::string calleeId;
    CallState   state;
    std::chrono::steady_clock::time_point createdAt;
};

using CallEventCallback = std::function<void(const std::string& userId,
                                              const std::string& json)>;

class CallManager {
public:
    explicit CallManager(std::chrono::seconds ring_timeout =
                         std::chrono::seconds(30));

    std::string create(const std::string& callerId,
                       const std::string& calleeId);

    CallSession accept(const std::string& callId,
                       const std::string& calleeId);

    CallSession reject(const std::string& callId,
                       const std::string& calleeId);

    CallSession hangup(const std::string& callId,
                       const std::string& userId);

    std::optional<CallSession> find(const std::string& callId) const;

    std::vector<std::string> expireRinging();

    bool isBusy(const std::string& userId) const;

private:
    static std::string generateUuid();

    std::chrono::seconds ring_timeout_;
    mutable std::mutex   mutex_;

    std::unordered_map<std::string, CallSession> sessions_;
};

}