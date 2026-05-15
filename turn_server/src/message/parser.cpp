#include "parser.hpp"
#include "xor_codec.hpp"
#include <cstring>

namespace message {

std::optional<Attribute> TurnMessage::findAttr(AttrType type) const {
    for (auto& a : attributes)
        if (a.type == type) return a;
    return std::nullopt;
}

Method decodeMethod(uint16_t msg_type) {
    uint16_t m = ((msg_type & 0x3E00) >> 2) |
                 ((msg_type & 0x00E0) >> 1) |
                  (msg_type & 0x000F);
    switch (m) {
        case 0x003: return Method::Allocate;
        case 0x004: return Method::Refresh;
        case 0x006: return Method::Send;
        case 0x008: return Method::CreatePermission;
        case 0x009: return Method::ChannelBind;
        default:    return Method::Unknown;
    }
}

MessageClass decodeClass(uint16_t msg_type) {
    uint8_t c = ((msg_type >> 7) & 0x02) | ((msg_type >> 4) & 0x01);
    switch (c) {
        case 0: return MessageClass::Request;
        case 1: return MessageClass::Indication;
        case 2: return MessageClass::SuccessResponse;
        case 3: return MessageClass::ErrorResponse;
        default: return MessageClass::Request;
    }
}

ParseResult parse(const uint8_t* data, size_t size, TurnMessage& out) {
    if (size < 20)
        return ParseResult::TooShort;

    if ((data[0] & 0xC0) != 0x00)
        return ParseResult::NotTurnMessage;

    uint16_t msg_type = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    out.method    = decodeMethod(msg_type);
    out.msg_class = decodeClass(msg_type);

    out.length = (static_cast<uint16_t>(data[2]) << 8) | data[3];

    if (size < 20u + out.length)
        return ParseResult::BadLength;

    uint32_t cookie = (static_cast<uint32_t>(data[4]) << 24) |
                      (static_cast<uint32_t>(data[5]) << 16) |
                      (static_cast<uint32_t>(data[6]) <<  8) |
                       static_cast<uint32_t>(data[7]);
    if (cookie != kMagicCookie)
        return ParseResult::BadMagicCookie;

    std::copy(data + 8, data + 20, out.transaction_id.begin());

    out.attributes.clear();
    size_t offset = 20;
    while (offset + 4 <= 20u + out.length) {
        Attribute attr;
        uint16_t type_raw = (static_cast<uint16_t>(data[offset])     << 8) |
                             static_cast<uint16_t>(data[offset + 1]);
        uint16_t attr_len = (static_cast<uint16_t>(data[offset + 2]) << 8) |
                             static_cast<uint16_t>(data[offset + 3]);
        attr.type = static_cast<AttrType>(type_raw);
        offset += 4;

        if (offset + attr_len > size) break;

        attr.value.assign(data + offset, data + offset + attr_len);
        out.attributes.push_back(std::move(attr));

        offset += attr_len;
        if (attr_len % 4 != 0)
            offset += 4 - (attr_len % 4);
    }

    return ParseResult::Ok;
}

bool is_channel_data(const uint8_t* data, size_t size) {
    if (size < 1) return false;
    return (data[0] & 0xC0) == 0x40;
}

ChannelDataResult parse_channel_data(const uint8_t*      raw,
                                      size_t              size,
                                      ChannelDataMessage& out) {
    if (size < 4)
        return ChannelDataResult::TooShort;

    uint16_t ch  = (static_cast<uint16_t>(raw[0]) << 8) | raw[1];
    uint16_t len = (static_cast<uint16_t>(raw[2]) << 8) | raw[3];

    if (ch < 0x4000 || ch > 0x4FFF)
        return ChannelDataResult::InvalidChannel;

    if (size < 4u + len)
        return ChannelDataResult::TooShort;

    out.channel_number = ch;
    out.data.assign(raw + 4, raw + 4 + len);
    return ChannelDataResult::Ok;
}

}