#pragma once

#include "allocation.hpp"
#include "message/parser.hpp"
#include "message/builder.hpp"
#include "transport/transport.hpp"

#include <boost/asio.hpp>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <functional>
#include <atomic>

namespace asio = boost::asio;
using udp      = asio::ip::udp;

namespace allocation {

using SendCallback = std::function<void(const std::vector<uint8_t>&,
                                        const transport::Endpoint&)>;

class AllocationManager {
public:
    AllocationManager(asio::io_context& ioc,
                      const std::string& relay_address,
                      uint16_t           relay_port_min,
                      uint16_t           relay_port_max,
                      uint32_t           max_alloc_per_user = 10);

    std::vector<uint8_t> handleAllocate(
        const message::TurnMessage&  msg,
        const transport::Endpoint&   client,
        const std::string&           username,
        const std::string&           realm);

    std::vector<uint8_t> handleRefresh(
        const message::TurnMessage& msg,
        const transport::Endpoint&  client,
        const std::string&          username);

    std::vector<uint8_t> handleCreatePermission(
        const message::TurnMessage& msg,
        const transport::Endpoint&  client);

    std::vector<uint8_t> handleChannelBind(
        const message::TurnMessage& msg,
        const transport::Endpoint&  client);

    void handleSend(const message::TurnMessage& msg,
                    const transport::Endpoint&  client);

    void handlePeerData(const uint8_t*             data,
                        size_t                     size,
                        const transport::Endpoint& peer,
                        const transport::Endpoint& relay_addr);

    void setClientSendCallback(SendCallback cb);

    void setPeerSendCallback(SendCallback cb);

    std::shared_ptr<Allocation> findByClient(
        const transport::Endpoint& client) const;

    std::shared_ptr<Allocation> findByRelay(
        const transport::Endpoint& relay) const;

private:
    std::optional<uint16_t> allocatePort();
    void                    releasePort(uint16_t port);
    uint32_t                requestedLifetime(
                                const message::TurnMessage& msg) const;

    asio::io_context& ioc_;
    std::string       relay_address_;
    uint16_t          relay_port_min_;
    uint16_t          relay_port_max_;
    uint32_t          max_alloc_per_user_;

    std::atomic<uint64_t> next_id_{1};

    mutable std::mutex mutex_;

    std::unordered_map<std::string,
                       std::shared_ptr<Allocation>> by_client_;
    std::unordered_map<std::string,
                       std::shared_ptr<Allocation>> by_relay_;
    std::unordered_map<uint16_t, bool>              used_ports_;

    SendCallback client_send_;
    SendCallback peer_send_;

    static std::string endpointKey(const transport::Endpoint& ep) {
        return ep.address + ":" + std::to_string(ep.port);
    }
};

}