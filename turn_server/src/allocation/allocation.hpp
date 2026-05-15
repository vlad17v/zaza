#pragma once

#include <string>
#include <cstdint>
#include <chrono>
#include <unordered_map>
#include <optional>

#include "transport/transport.hpp"

namespace allocation {

using Clock     = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

struct Permission {
    std::string address;
    TimePoint   expiresAt;
};

struct ChannelBinding {
    uint16_t              channelNumber;
    transport::Endpoint   peer;
    TimePoint             expiresAt;
};

struct Allocation {
    uint64_t              id;
    transport::Endpoint   clientAddr;
    transport::Endpoint   relayedAddr;
    std::string           username;
    std::string           realm;

    TimePoint             expiresAt;

    static constexpr uint32_t kDefaultLifetime = 600;
    static constexpr uint32_t kMaxLifetime     = 3600;
    static constexpr uint32_t kPermissionTTL   = 300;
    static constexpr uint32_t kChannelTTL      = 600;

    std::unordered_map<std::string, Permission>       permissions;
    std::unordered_map<uint16_t,    ChannelBinding>   channels;

    bool isExpired() const {
        return Clock::now() >= expiresAt;
    }

    bool hasPermission(const std::string& peerAddress) const {
        auto it = permissions.find(peerAddress);
        if (it == permissions.end()) return false;
        return Clock::now() < it->second.expiresAt;
    }

    std::optional<ChannelBinding> findChannelByPeer(
        const transport::Endpoint& peer) const {
        for (auto& [num, ch] : channels) {
            if (ch.peer.address == peer.address &&
                ch.peer.port    == peer.port &&
                Clock::now() < ch.expiresAt)
                return ch;
        }
        return std::nullopt;
    }

    std::optional<ChannelBinding> findChannelByNumber(uint16_t num) const {
        auto it = channels.find(num);
        if (it == channels.end()) return std::nullopt;
        if (Clock::now() >= it->second.expiresAt) return std::nullopt;
        return it->second;
    }
};

}