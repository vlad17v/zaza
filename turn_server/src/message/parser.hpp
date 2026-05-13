#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <optional>
#include <array>

namespace message {

enum class MessageClass : uint8_t {
    Request         = 0x00,
    Indication      = 0x01,
    SuccessResponse = 0x02,
    ErrorResponse   = 0x03,
};

enum class Method : uint16_t {
    Allocate         = 0x003,
    Refresh          = 0x004,
    Send             = 0x006,
    CreatePermission = 0x008,
    ChannelBind      = 0x009,
    Unknown          = 0xFFF,
};

enum class AttrType : uint16_t {
    MappedAddress          = 0x0001,
    Username               = 0x0006,
    MessageIntegrity       = 0x0008,
    ErrorCode              = 0x0009,
    UnknownAttributes      = 0x000A,
    ChannelNumber          = 0x000C,
    Lifetime               = 0x000D,
    XorPeerAddress         = 0x0012,
    Data                   = 0x0013,
    Realm                  = 0x0014,
    Nonce                  = 0x0015,
    XorRelayedAddress      = 0x0016,
    RequestedTransport     = 0x0019,
    MessageIntegritySha256 = 0x001C,
    PasswordAlgorithm      = 0x001D,
    XorMappedAddress       = 0x0020,
    PasswordAlgorithms     = 0x8002,
    Software               = 0x8022,
    Fingerprint            = 0x8028,
};

struct Attribute {
    AttrType             type;
    std::vector<uint8_t> value;
};

struct TurnMessage {
    MessageClass            msg_class;
    Method                  method;
    uint16_t                length;
    std::array<uint8_t, 12> transaction_id;
    std::vector<Attribute>  attributes;

    std::optional<Attribute> findAttr(AttrType type) const;
};

enum class ParseResult {
    Ok,
    TooShort,
    BadMagicCookie,
    BadLength,
    NotTurnMessage,
};

ParseResult  parse(const uint8_t* data, size_t size, TurnMessage& out);
Method       decodeMethod(uint16_t msg_type);
MessageClass decodeClass(uint16_t msg_type);

struct ChannelDataMessage {
    uint16_t             channel_number;
    std::vector<uint8_t> data;
};

enum class ChannelDataResult {
    Ok,
    TooShort,
    InvalidChannel,
};

ChannelDataResult parse_channel_data(const uint8_t*      raw,
                                      size_t              size,
                                      ChannelDataMessage& out);

bool is_channel_data(const uint8_t* data, size_t size);

}