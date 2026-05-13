#pragma once

#include "parser.hpp"
#include <vector>
#include <array>
#include <cstdint>
#include <string>

namespace message {

class MessageBuilder {
public:
    MessageBuilder(Method                         method,
                   MessageClass                   msg_class,
                   const std::array<uint8_t, 12>& transaction_id);

    MessageBuilder& addAttr(AttrType type, std::vector<uint8_t> value);
    MessageBuilder& addErrorCode(uint16_t code, const std::string& reason);
    MessageBuilder& addLifetime(uint32_t seconds);
    MessageBuilder& addXorMappedAddress(uint32_t ipv4, uint16_t port);
    MessageBuilder& addXorRelayedAddress(uint32_t ipv4, uint16_t port);

    std::vector<uint8_t> build() const;

private:
    void appendAttrRaw(uint16_t type, const uint8_t* value, uint16_t len);
    void appendXorAddress(AttrType type, uint32_t ipv4, uint16_t port);

    static uint16_t encodeType(Method method, MessageClass msg_class);

    uint16_t                msg_type_;
    std::array<uint8_t, 12> transaction_id_;
    std::vector<uint8_t>    attrs_buf_;
};

std::vector<uint8_t> make400(const std::array<uint8_t, 12>& tid);
std::vector<uint8_t> make401(const std::array<uint8_t, 12>& tid,
                              const std::string& realm,
                              const std::string& nonce);
std::vector<uint8_t> make438(const std::array<uint8_t, 12>& tid,
                              const std::string& realm,
                              const std::string& nonce);

std::vector<uint8_t> make_data_indication(
    const std::array<uint8_t, 12>& tid,
    uint32_t                        peer_ipv4,
    uint16_t                        peer_port,
    const std::vector<uint8_t>&     data);

std::vector<uint8_t> make_channel_data(uint16_t                    channel_number,
                                        const std::vector<uint8_t>& data);

}