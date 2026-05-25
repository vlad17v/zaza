#include "allocation_manager.hpp"
#include "permission_table.hpp"
#include "message/xor_codec.hpp"

#include <random>
#include <iostream>

namespace allocation {

AllocationManager::AllocationManager(asio::io_context&  ioc,
                                      const std::string& relay_address,
                                      uint16_t           relay_port_min,
                                      uint16_t           relay_port_max,
                                      uint32_t           max_alloc_per_user)
    : ioc_(ioc)
    , relay_address_(relay_address)
    , relay_port_min_(relay_port_min)
    , relay_port_max_(relay_port_max)
    , max_alloc_per_user_(max_alloc_per_user)
{}

void AllocationManager::setClientSendCallback(SendCallback cb) {
    client_send_ = std::move(cb);
}

void AllocationManager::setPeerSendCallback(SendCallback cb) {
    peer_send_ = std::move(cb);
}

std::optional<uint16_t> AllocationManager::allocatePort() {
    static std::mt19937 rng(std::random_device{}());

    uint16_t range = relay_port_max_ - relay_port_min_ + 1;
    std::uniform_int_distribution<uint16_t> dist(0, range - 1);

    for (int attempts = 0; attempts < 1000; ++attempts) {
        uint16_t port = relay_port_min_ + dist(rng);
        if (!used_ports_[port]) {
            used_ports_[port] = true;
            return port;
        }
    }
    return std::nullopt;
}

void AllocationManager::releasePort(uint16_t port) {
    used_ports_.erase(port);
}

uint32_t AllocationManager::requestedLifetime(
    const message::TurnMessage& msg) const
{
    auto lt = msg.findAttr(message::AttrType::Lifetime);
    if (!lt || lt->value.size() < 4)
        return Allocation::kDefaultLifetime;

    uint32_t requested =
        (static_cast<uint32_t>(lt->value[0]) << 24) |
        (static_cast<uint32_t>(lt->value[1]) << 16) |
        (static_cast<uint32_t>(lt->value[2]) <<  8) |
         static_cast<uint32_t>(lt->value[3]);

    if (requested == 0) return 0;
    return std::min(requested, Allocation::kMaxLifetime);
}

std::shared_ptr<Allocation> AllocationManager::findByClient(
    const transport::Endpoint& client) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = by_client_.find(endpointKey(client));
    if (it == by_client_.end()) return nullptr;
    return it->second;
}

std::shared_ptr<Allocation> AllocationManager::findByRelay(
    const transport::Endpoint& relay) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = by_relay_.find(endpointKey(relay));
    if (it == by_relay_.end()) return nullptr;
    return it->second;
}

std::vector<uint8_t> AllocationManager::handleAllocate(
    const message::TurnMessage& msg,
    const transport::Endpoint&  client,
    const std::string&          username,
    const std::string&          realm)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (by_client_.count(endpointKey(client)))
        return message::make_error(msg.transaction_id, 437,
                                   "Allocation Mismatch");

    auto rt = msg.findAttr(message::AttrType::RequestedTransport);
    if (!rt || rt->value.size() < 4 || rt->value[0] != 17)
        return rt
            ? message::make_error(msg.transaction_id, 442,
                                   "Unsupported Transport Protocol")
            : message::make_error(msg.transaction_id, 400,
                                   "Bad Request");

    uint32_t user_count = 0;
    for (auto& [k, a] : by_client_)
        if (a->username == username) ++user_count;
    if (user_count >= max_alloc_per_user_)
        return message::make_error(msg.transaction_id, 486,
                                   "Allocation Quota Reached");

    auto port_opt = allocatePort();
    if (!port_opt)
        return message::make_error(msg.transaction_id, 508,
                                   "Insufficient Capacity");

    uint16_t relay_port = *port_opt;

    uint32_t lifetime = requestedLifetime(msg);
    if (lifetime == 0) lifetime = Allocation::kDefaultLifetime;

    auto alloc          = std::make_shared<Allocation>();
    alloc->id           = next_id_++;
    alloc->clientAddr   = client;
    alloc->relayedAddr  = {relay_address_, relay_port};
    alloc->username     = username;
    alloc->realm        = realm;
    alloc->expiresAt    = Clock::now() +
                          std::chrono::seconds(lifetime);

    by_client_[endpointKey(client)]               = alloc;
    by_relay_[endpointKey(alloc->relayedAddr)]    = alloc;

    boost::system::error_code ec;
    auto relay_v4 = boost::asio::ip::make_address_v4(relay_address_, ec);
    uint32_t relay_ip = ec ? 0 : relay_v4.to_uint();

    auto client_v4 = boost::asio::ip::make_address_v4(client.address, ec);
    uint32_t client_ip = ec ? 0 : client_v4.to_uint();

    return message::MessageBuilder(
               message::Method::Allocate,
               message::MessageClass::SuccessResponse,
               msg.transaction_id)
        .addXorRelayedAddress(relay_ip, relay_port)
        .addXorMappedAddress(client_ip, client.port)
        .addLifetime(lifetime)
        .build();
}

std::vector<uint8_t> AllocationManager::handleRefresh(
    const message::TurnMessage& msg,
    const transport::Endpoint&  client,
    const std::string&          username)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = by_client_.find(endpointKey(client));
    if (it == by_client_.end())
        return message::make_error(msg.transaction_id, 437,
                                   "Allocation Mismatch");

    auto& alloc = it->second;

    if (alloc->username != username)
        return message::make_error(msg.transaction_id, 441,
                                   "Wrong Credentials");

    uint32_t lifetime = requestedLifetime(msg);

    if (lifetime == 0) {
        by_relay_.erase(endpointKey(alloc->relayedAddr));
        releasePort(alloc->relayedAddr.port);
        by_client_.erase(it);
        return message::MessageBuilder(
                   message::Method::Refresh,
                   message::MessageClass::SuccessResponse,
                   msg.transaction_id)
            .addLifetime(0)
            .build();
    }

    alloc->expiresAt = Clock::now() +
                       std::chrono::seconds(lifetime);

    return message::MessageBuilder(
               message::Method::Refresh,
               message::MessageClass::SuccessResponse,
               msg.transaction_id)
        .addLifetime(lifetime)
        .build();
}

std::vector<uint8_t> AllocationManager::handleCreatePermission(
    const message::TurnMessage& msg,
    const transport::Endpoint&  client)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = by_client_.find(endpointKey(client));
    if (it == by_client_.end())
        return message::make_error(msg.transaction_id, 437,
                                   "Allocation Mismatch");

    auto& alloc = it->second;

    bool found_peer = false;
    for (auto& attr : msg.attributes) {
        if (attr.type != message::AttrType::XorPeerAddress) continue;
        if (attr.value.size() < 8) continue;

        uint16_t xport = (static_cast<uint16_t>(attr.value[2]) << 8) |
                          attr.value[3];
        uint32_t xaddr = (static_cast<uint32_t>(attr.value[4]) << 24) |
                         (static_cast<uint32_t>(attr.value[5]) << 16) |
                         (static_cast<uint32_t>(attr.value[6]) <<  8) |
                          static_cast<uint32_t>(attr.value[7]);

        uint32_t ip   = xaddr ^ message::kMagicCookie;
        boost::asio::ip::address_v4 addr_v4(ip);
        std::string peer_address = addr_v4.to_string();

        if (isDeniedAddress(peer_address))
            return message::make_error(msg.transaction_id, 403, "Forbidden");

        Permission perm;
        perm.address   = peer_address;
        perm.expiresAt = Clock::now() +
                         std::chrono::seconds(Allocation::kPermissionTTL);
        alloc->permissions[peer_address] = perm;
        found_peer = true;
        (void)xport;
    }

    if (!found_peer)
        return message::make_error(msg.transaction_id, 400, "Bad Request");

    return message::MessageBuilder(
               message::Method::CreatePermission,
               message::MessageClass::SuccessResponse,
               msg.transaction_id)
        .build();
}

std::vector<uint8_t> AllocationManager::handleChannelBind(
    const message::TurnMessage& msg,
    const transport::Endpoint&  client)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = by_client_.find(endpointKey(client));
    if (it == by_client_.end())
        return message::make_error(msg.transaction_id, 437,
                                   "Allocation Mismatch");

    auto& alloc = it->second;

    auto ch_attr   = msg.findAttr(message::AttrType::ChannelNumber);
    auto peer_attr = msg.findAttr(message::AttrType::XorPeerAddress);

    if (!ch_attr || !peer_attr ||
        ch_attr->value.size() < 2 || peer_attr->value.size() < 8)
        return message::make_error(msg.transaction_id, 400, "Bad Request");

    uint16_t channel_num =
        (static_cast<uint16_t>(ch_attr->value[0]) << 8) |
         ch_attr->value[1];

    if (channel_num < 0x4000 || channel_num > 0x4FFF)
        return message::make_error(msg.transaction_id, 400, "Bad Request");

    uint32_t xaddr = (static_cast<uint32_t>(peer_attr->value[4]) << 24) |
                     (static_cast<uint32_t>(peer_attr->value[5]) << 16) |
                     (static_cast<uint32_t>(peer_attr->value[6]) <<  8) |
                      static_cast<uint32_t>(peer_attr->value[7]);
    uint16_t xport = (static_cast<uint16_t>(peer_attr->value[2]) << 8) |
                      peer_attr->value[3];

    uint32_t ip   = xaddr ^ message::kMagicCookie;
    uint16_t port = xport ^ static_cast<uint16_t>(message::kMagicCookie >> 16);

    boost::asio::ip::address_v4 addr_v4(ip);
    transport::Endpoint peer{addr_v4.to_string(), port};

    if (isDeniedAddress(peer.address))
        return message::make_error(msg.transaction_id, 403, "Forbidden");

    auto existing_ch = alloc->channels.find(channel_num);
    if (existing_ch != alloc->channels.end()) {
        if (existing_ch->second.peer.address != peer.address ||
            existing_ch->second.peer.port    != peer.port)
            return message::make_error(msg.transaction_id, 400, "Bad Request");
    }

    for (auto& [num, ch] : alloc->channels) {
        if (num == channel_num) continue;
        if (ch.peer.address == peer.address &&
            ch.peer.port    == peer.port)
            return message::make_error(msg.transaction_id, 400, "Bad Request");
    }

    ChannelBinding binding;
    binding.channelNumber = channel_num;
    binding.peer          = peer;
    binding.expiresAt     = Clock::now() +
                            std::chrono::seconds(Allocation::kChannelTTL);
    alloc->channels[channel_num] = binding;

    Permission perm;
    perm.address   = peer.address;
    perm.expiresAt = Clock::now() +
                     std::chrono::seconds(Allocation::kPermissionTTL);
    alloc->permissions[peer.address] = perm;

    return message::MessageBuilder(
               message::Method::ChannelBind,
               message::MessageClass::SuccessResponse,
               msg.transaction_id)
        .build();
}

void AllocationManager::handleSend(
    const message::TurnMessage& msg,
    const transport::Endpoint&  client)
{
    std::cout << "[alloc] handleSend from " << client.address << ":" << client.port << "\n";

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = by_client_.find(endpointKey(client));
    if (it == by_client_.end()) {
        std::cout << "[alloc] no allocation for client\n";
        return;
    }
    auto& alloc = it->second;

    auto peer_attr = msg.findAttr(message::AttrType::XorPeerAddress);
    auto data_attr = msg.findAttr(message::AttrType::Data);

    if (!peer_attr || peer_attr->value.size() < 8) {
        std::cout << "[alloc] no peer attr\n";
        return;
    }

    uint32_t xaddr = (static_cast<uint32_t>(peer_attr->value[4]) << 24) |
                     (static_cast<uint32_t>(peer_attr->value[5]) << 16) |
                     (static_cast<uint32_t>(peer_attr->value[6]) <<  8) |
                      static_cast<uint32_t>(peer_attr->value[7]);
    uint16_t xport = (static_cast<uint16_t>(peer_attr->value[2]) << 8) |
                      peer_attr->value[3];
    uint32_t ip   = xaddr ^ message::kMagicCookie;
    uint16_t port = xport ^ static_cast<uint16_t>(message::kMagicCookie >> 16);
    boost::asio::ip::address_v4 addr_v4(ip);
    transport::Endpoint peer{addr_v4.to_string(), port};

    std::cout << "[alloc] peer=" << peer.address << ":" << peer.port
              << " perm=" << alloc->hasPermission(peer.address) << "\n";

    if (!alloc->hasPermission(peer.address)) return;
    if (!data_attr || !peer_send_) {
        std::cout << "[alloc] no data or no peer_send_\n";
        return;
    }

    peer_send_(data_attr->value, peer);
}

void AllocationManager::handlePeerData(
    const uint8_t*             data,
    size_t                     size,
    const transport::Endpoint& peer,
    const transport::Endpoint& relay_addr)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = by_relay_.find(endpointKey(relay_addr));
    if (it == by_relay_.end()) return;

    auto& alloc = it->second;

    if (!alloc->hasPermission(peer.address)) return;

    if (!client_send_) return;

    auto ch = alloc->findChannelByPeer(peer);
    if (ch) {
        std::vector<uint8_t> payload(data, data + size);
        auto msg = message::make_channel_data(ch->channelNumber, payload);
        client_send_(msg, alloc->clientAddr);
        return;
    }

    boost::system::error_code ec;
    auto peer_v4 = boost::asio::ip::make_address_v4(peer.address, ec);
    uint32_t peer_ip = ec ? 0 : peer_v4.to_uint();

    std::vector<uint8_t> payload(data, data + size);
    std::array<uint8_t, 12> tid{};
    auto indication = message::make_data_indication(tid, peer_ip,
                                                     peer.port, payload);
    client_send_(indication, alloc->clientAddr);
}

}