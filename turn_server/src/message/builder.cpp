#include "builder.hpp"
#include "xor_codec.hpp"

namespace message {

uint16_t MessageBuilder::encodeType(Method method, MessageClass msg_class) {
    uint16_t m = static_cast<uint16_t>(method);
    uint16_t c = static_cast<uint16_t>(msg_class);

    uint16_t m_high = (m & 0x0F80) << 2;
    uint16_t m_mid  = (m & 0x0070) << 1;
    uint16_t m_low  =  m & 0x000F;

    uint16_t c_high = (c & 0x02) << 7;
    uint16_t c_low  = (c & 0x01) << 4;

    return m_high | c_high | m_mid | c_low | m_low;
}

MessageBuilder::MessageBuilder(Method                         method,
                                MessageClass                   msg_class,
                                const std::array<uint8_t, 12>& tid)
    : msg_type_(encodeType(method, msg_class))
    , transaction_id_(tid)
{}

void MessageBuilder::appendAttrRaw(uint16_t       type,
                                    const uint8_t* value,
                                    uint16_t       len) {
    attrs_buf_.push_back((type >> 8) & 0xFF);
    attrs_buf_.push_back( type       & 0xFF);
    attrs_buf_.push_back((len  >> 8) & 0xFF);
    attrs_buf_.push_back( len        & 0xFF);
    attrs_buf_.insert(attrs_buf_.end(), value, value + len);
    if (len % 4 != 0) {
        uint8_t pad = 4 - (len % 4);
        attrs_buf_.insert(attrs_buf_.end(), pad, 0x00);
    }
}

MessageBuilder& MessageBuilder::addAttr(AttrType             type,
                                         std::vector<uint8_t> value) {
    appendAttrRaw(static_cast<uint16_t>(type),
                  value.data(),
                  static_cast<uint16_t>(value.size()));
    return *this;
}

MessageBuilder& MessageBuilder::addErrorCode(uint16_t           code,
                                              const std::string& reason) {
    std::vector<uint8_t> buf(4 + reason.size());
    buf[0] = 0x00;
    buf[1] = 0x00;
    buf[2] = static_cast<uint8_t>(code / 100) & 0x07;
    buf[3] = static_cast<uint8_t>(code % 100);
    std::copy(reason.begin(), reason.end(), buf.begin() + 4);
    return addAttr(AttrType::ErrorCode, std::move(buf));
}

MessageBuilder& MessageBuilder::addLifetime(uint32_t seconds) {
    std::vector<uint8_t> buf(4);
    buf[0] = (seconds >> 24) & 0xFF;
    buf[1] = (seconds >> 16) & 0xFF;
    buf[2] = (seconds >>  8) & 0xFF;
    buf[3] =  seconds        & 0xFF;
    return addAttr(AttrType::Lifetime, std::move(buf));
}

void MessageBuilder::appendXorAddress(AttrType type,
                                       uint32_t ipv4,
                                       uint16_t port) {
    std::vector<uint8_t> buf(8);
    buf[0] = 0x00;
    buf[1] = 0x01;
    uint16_t xport = xor_port(port);
    uint32_t xaddr = xor_addr_v4(ipv4);
    buf[2] = (xport >> 8) & 0xFF;
    buf[3] =  xport       & 0xFF;
    buf[4] = (xaddr >> 24) & 0xFF;
    buf[5] = (xaddr >> 16) & 0xFF;
    buf[6] = (xaddr >>  8) & 0xFF;
    buf[7] =  xaddr        & 0xFF;
    addAttr(type, std::move(buf));
}

MessageBuilder& MessageBuilder::addXorMappedAddress(uint32_t ipv4,
                                                     uint16_t port) {
    appendXorAddress(AttrType::XorMappedAddress, ipv4, port);
    return *this;
}

MessageBuilder& MessageBuilder::addXorRelayedAddress(uint32_t ipv4,
                                                      uint16_t port) {
    appendXorAddress(AttrType::XorRelayedAddress, ipv4, port);
    return *this;
}

std::vector<uint8_t> MessageBuilder::build() const {
    uint16_t attrs_len = static_cast<uint16_t>(attrs_buf_.size());
    std::vector<uint8_t> out;
    out.reserve(20 + attrs_len);

    out.push_back((msg_type_ >> 8) & 0xFF);
    out.push_back( msg_type_       & 0xFF);
    out.push_back((attrs_len >> 8) & 0xFF);
    out.push_back( attrs_len       & 0xFF);
    out.push_back(0x21);
    out.push_back(0x12);
    out.push_back(0xA4);
    out.push_back(0x42);
    out.insert(out.end(), transaction_id_.begin(), transaction_id_.end());
    out.insert(out.end(), attrs_buf_.begin(), attrs_buf_.end());

    return out;
}

std::vector<uint8_t> make400(const std::array<uint8_t, 12>& tid) {
    return MessageBuilder(Method::Allocate, MessageClass::ErrorResponse, tid)
        .addErrorCode(400, "Bad Request")
        .build();
}

std::vector<uint8_t> make401(const std::array<uint8_t, 12>& tid,
                               const std::string& realm,
                               const std::string& nonce) {
    std::vector<uint8_t> realm_v(realm.begin(), realm.end());
    std::vector<uint8_t> nonce_v(nonce.begin(), nonce.end());
    return MessageBuilder(Method::Allocate, MessageClass::ErrorResponse, tid)
        .addErrorCode(401, "Unauthorized")
        .addAttr(AttrType::Realm, std::move(realm_v))
        .addAttr(AttrType::Nonce, std::move(nonce_v))
        .build();
}

std::vector<uint8_t> make438(const std::array<uint8_t, 12>& tid,
                               const std::string& realm,
                               const std::string& nonce) {
    std::vector<uint8_t> realm_v(realm.begin(), realm.end());
    std::vector<uint8_t> nonce_v(nonce.begin(), nonce.end());
    return MessageBuilder(Method::Allocate, MessageClass::ErrorResponse, tid)
        .addErrorCode(438, "Stale Nonce")
        .addAttr(AttrType::Realm, std::move(realm_v))
        .addAttr(AttrType::Nonce, std::move(nonce_v))
        .build();
}

std::vector<uint8_t> make_data_indication(
    const std::array<uint8_t, 12>& tid,
    uint32_t                        peer_ipv4,
    uint16_t                        peer_port,
    const std::vector<uint8_t>&     payload)
{
    return MessageBuilder(Method::Send, MessageClass::Indication, tid)
        .addXorMappedAddress(peer_ipv4, peer_port)
        .addAttr(AttrType::Data,
                 std::vector<uint8_t>(payload.begin(), payload.end()))
        .build();
}

std::vector<uint8_t> make_channel_data(uint16_t                    channel_number,
                                        const std::vector<uint8_t>& data)
{
    uint16_t len = static_cast<uint16_t>(data.size());

    std::vector<uint8_t> out;
    out.reserve(4 + data.size());

    out.push_back((channel_number >> 8) & 0xFF);
    out.push_back( channel_number       & 0xFF);
    out.push_back((len >> 8) & 0xFF);
    out.push_back( len       & 0xFF);
    out.insert(out.end(), data.begin(), data.end());

    if (len % 4 != 0) {
        uint8_t pad = 4 - (len % 4);
        out.insert(out.end(), pad, 0x00);
    }

    return out;
}

}