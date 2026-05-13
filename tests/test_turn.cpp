#include "message/parser.hpp"
#include "message/builder.hpp"
#include "message/xor_codec.hpp"

#include <iostream>
#include <cassert>
#include <cstring>
#include <vector>
#include <array>

static std::array<uint8_t, 12> make_tid(uint8_t fill = 0xAB) {
    std::array<uint8_t, 12> tid;
    tid.fill(fill);
    return tid;
}

static std::vector<uint8_t> make_allocate_request(
    const std::array<uint8_t, 12>& tid)
{
    std::vector<uint8_t> msg(20);
    msg[0] = 0x00;
    msg[1] = 0x03;
    msg[2] = 0x00;
    msg[3] = 0x00;
    msg[4] = 0x21; msg[5] = 0x12; msg[6] = 0xA4; msg[7] = 0x42;
    std::copy(tid.begin(), tid.end(), msg.begin() + 8);
    return msg;
}

int main() {
    std::cout << "=== XOR codec ===\n";

    {
        uint16_t port  = 3478;
        uint16_t xport = message::xor_port(port);
        assert(xport != port);
        assert(message::xor_port(xport) == port);
        std::cout << "[xor] port round-trip OK\n";
    }

    {
        uint32_t addr  = 0xC0000201;
        uint32_t xaddr = message::xor_addr_v4(addr);
        assert(xaddr != addr);
        assert(message::xor_addr_v4(xaddr) == addr);
        std::cout << "[xor] addr round-trip OK\n";
    }

    {
        assert(message::xor_port(0) == static_cast<uint16_t>(message::kMagicCookie >> 16));
        assert(message::xor_addr_v4(0) == message::kMagicCookie);
        std::cout << "[xor] zero values OK\n";
    }

    std::cout << "\n=== Parser ===\n";

    {
        auto tid = make_tid();
        auto raw = make_allocate_request(tid);
        message::TurnMessage msg;
        auto r = message::parse(raw.data(), raw.size(), msg);
        assert(r == message::ParseResult::Ok);
        assert(msg.method    == message::Method::Allocate);
        assert(msg.msg_class == message::MessageClass::Request);
        assert(msg.length    == 0);
        assert(msg.transaction_id == tid);
        assert(msg.attributes.empty());
        std::cout << "[parser] Allocate request OK\n";
    }

    {
        std::vector<uint8_t> short_msg(19, 0x00);
        message::TurnMessage msg;
        assert(message::parse(short_msg.data(), short_msg.size(), msg)
               == message::ParseResult::TooShort);
        std::cout << "[parser] too short OK\n";
    }

    {
        std::vector<uint8_t> empty;
        message::TurnMessage msg;
        assert(message::parse(empty.data(), 0, msg)
               == message::ParseResult::TooShort);
        std::cout << "[parser] empty OK\n";
    }

    {
        auto raw = make_allocate_request(make_tid());
        raw[0] = 0x80;
        message::TurnMessage msg;
        assert(message::parse(raw.data(), raw.size(), msg)
               == message::ParseResult::NotTurnMessage);
        std::cout << "[parser] not TURN message (top bits) OK\n";
    }

    {
        auto raw = make_allocate_request(make_tid());
        raw[5] = 0x00;
        message::TurnMessage msg;
        assert(message::parse(raw.data(), raw.size(), msg)
               == message::ParseResult::BadMagicCookie);
        std::cout << "[parser] bad magic cookie OK\n";
    }

    {
        auto raw = make_allocate_request(make_tid());
        raw[2] = 0x01;
        raw[3] = 0x00;
        message::TurnMessage msg;
        assert(message::parse(raw.data(), raw.size(), msg)
               == message::ParseResult::BadLength);
        std::cout << "[parser] bad length OK\n";
    }

    {
        auto tid = make_tid(0x01);
        std::vector<uint8_t> raw(20 + 8, 0x00);
        raw[0] = 0x00; raw[1] = 0x03;
        raw[2] = 0x00; raw[3] = 0x08;
        raw[4] = 0x21; raw[5] = 0x12; raw[6] = 0xA4; raw[7] = 0x42;
        std::copy(tid.begin(), tid.end(), raw.begin() + 8);
        raw[20] = 0x00; raw[21] = 0x19;
        raw[22] = 0x00; raw[23] = 0x04;
        raw[24] = 0x00; raw[25] = 0x00; raw[26] = 0x00; raw[27] = 0x11;

        message::TurnMessage msg;
        auto r = message::parse(raw.data(), raw.size(), msg);
        assert(r == message::ParseResult::Ok);
        assert(msg.attributes.size() == 1);
        assert(msg.attributes[0].type == message::AttrType::RequestedTransport);
        assert(msg.attributes[0].value.size() == 4);
        assert(msg.attributes[0].value[3] == 0x11);
        std::cout << "[parser] REQUESTED-TRANSPORT attr OK\n";
    }

    {
        auto tid = make_tid();
        auto raw = make_allocate_request(tid);
        message::TurnMessage msg;
        message::parse(raw.data(), raw.size(), msg);
        assert(!msg.findAttr(message::AttrType::Username).has_value());
        std::cout << "[parser] findAttr missing OK\n";
    }

    {
        auto tid = make_tid(0x02);
        std::vector<uint8_t> raw(20 + 8, 0x00);
        raw[0] = 0x00; raw[1] = 0x03;
        raw[2] = 0x00; raw[3] = 0x08;
        raw[4] = 0x21; raw[5] = 0x12; raw[6] = 0xA4; raw[7] = 0x42;
        std::copy(tid.begin(), tid.end(), raw.begin() + 8);
        raw[20] = 0x00; raw[21] = 0x19;
        raw[22] = 0x00; raw[23] = 0x04;
        raw[24] = 0x00; raw[25] = 0x00; raw[26] = 0x00; raw[27] = 0x11;

        message::TurnMessage msg;
        message::parse(raw.data(), raw.size(), msg);
        auto attr = msg.findAttr(message::AttrType::RequestedTransport);
        assert(attr.has_value());
        assert(attr->value[3] == 0x11);
        std::cout << "[parser] findAttr found OK\n";
    }

    std::cout << "\n=== decodeMethod / decodeClass ===\n";

    {
        assert(message::decodeMethod(0x0003) == message::Method::Allocate);
        assert(message::decodeMethod(0x0004) == message::Method::Refresh);
        assert(message::decodeMethod(0x0006) == message::Method::Send);
        assert(message::decodeMethod(0x0008) == message::Method::CreatePermission);
        assert(message::decodeMethod(0x0009) == message::Method::ChannelBind);
        assert(message::decodeMethod(0xFFFF) == message::Method::Unknown);
        std::cout << "[decode] methods OK\n";
    }

    {
        assert(message::decodeClass(0x0000) == message::MessageClass::Request);
        assert(message::decodeClass(0x0010) == message::MessageClass::Indication);
        assert(message::decodeClass(0x0100) == message::MessageClass::SuccessResponse);
        assert(message::decodeClass(0x0110) == message::MessageClass::ErrorResponse);
        std::cout << "[decode] classes OK\n";
    }

    std::cout << "\n=== Builder ===\n";

    {
        auto tid = make_tid();
        auto raw = message::make400(tid);
        assert(raw.size() >= 20);
        assert((raw[0] & 0xC0) == 0x00);
        assert(raw[4] == 0x21 && raw[5] == 0x12 &&
               raw[6] == 0xA4 && raw[7] == 0x42);

        message::TurnMessage msg;
        auto r = message::parse(raw.data(), raw.size(), msg);
        assert(r == message::ParseResult::Ok);
        assert(msg.msg_class == message::MessageClass::ErrorResponse);
        assert(msg.transaction_id == tid);

        auto err = msg.findAttr(message::AttrType::ErrorCode);
        assert(err.has_value());
        assert(err->value[2] == 4);
        assert(err->value[3] == 0);
        std::cout << "[builder] make400 OK\n";
    }

    {
        auto tid = make_tid(0x11);
        auto raw = message::make401(tid, "chat.example.com", "testnonce123");
        message::TurnMessage msg;
        assert(message::parse(raw.data(), raw.size(), msg) == message::ParseResult::Ok);
        assert(msg.msg_class == message::MessageClass::ErrorResponse);

        auto err = msg.findAttr(message::AttrType::ErrorCode);
        assert(err.has_value());
        assert(err->value[2] == 4);
        assert(err->value[3] == 1);

        auto realm = msg.findAttr(message::AttrType::Realm);
        assert(realm.has_value());
        std::string realm_str(realm->value.begin(), realm->value.end());
        assert(realm_str == "chat.example.com");

        auto nonce = msg.findAttr(message::AttrType::Nonce);
        assert(nonce.has_value());
        std::string nonce_str(nonce->value.begin(), nonce->value.end());
        assert(nonce_str == "testnonce123");
        std::cout << "[builder] make401 OK\n";
    }

    {
        auto tid = make_tid(0x22);
        auto raw = message::make438(tid, "chat.example.com", "newnonce456");
        message::TurnMessage msg;
        assert(message::parse(raw.data(), raw.size(), msg) == message::ParseResult::Ok);

        auto err = msg.findAttr(message::AttrType::ErrorCode);
        assert(err.has_value());
        assert(err->value[2] == 4);
        assert(err->value[3] == 38);
        std::cout << "[builder] make438 OK\n";
    }

    {
        auto tid = make_tid(0x33);
        auto raw = message::MessageBuilder(
                       message::Method::Allocate,
                       message::MessageClass::SuccessResponse, tid)
            .addLifetime(600)
            .addXorMappedAddress(0xC0000201, 54321)
            .addXorRelayedAddress(0xC0000215, 50000)
            .build();

        message::TurnMessage msg;
        assert(message::parse(raw.data(), raw.size(), msg) == message::ParseResult::Ok);
        assert(msg.msg_class == message::MessageClass::SuccessResponse);
        assert(msg.method    == message::Method::Allocate);

        auto lifetime = msg.findAttr(message::AttrType::Lifetime);
        assert(lifetime.has_value());
        uint32_t lt = (static_cast<uint32_t>(lifetime->value[0]) << 24) |
                      (static_cast<uint32_t>(lifetime->value[1]) << 16) |
                      (static_cast<uint32_t>(lifetime->value[2]) <<  8) |
                       static_cast<uint32_t>(lifetime->value[3]);
        assert(lt == 600);

        auto mapped = msg.findAttr(message::AttrType::XorMappedAddress);
        assert(mapped.has_value());
        assert(mapped->value[1] == 0x01);

        auto relayed = msg.findAttr(message::AttrType::XorRelayedAddress);
        assert(relayed.has_value());
        assert(relayed->value[1] == 0x01);

        std::cout << "[builder] success response with attrs OK\n";
    }

    {
        auto tid = make_tid(0x44);
        auto raw = message::MessageBuilder(
                       message::Method::Allocate,
                       message::MessageClass::SuccessResponse, tid)
            .addLifetime(600)
            .build();

        message::TurnMessage msg;
        assert(message::parse(raw.data(), raw.size(), msg) == message::ParseResult::Ok);
        assert(msg.transaction_id == tid);
        std::cout << "[builder] transaction_id preserved OK\n";
    }

    {
        auto tid = make_tid();
        auto raw = message::MessageBuilder(
                       message::Method::Allocate,
                       message::MessageClass::ErrorResponse, tid)
            .build();
        assert(raw.size() == 20);
        std::cout << "[builder] empty attrs size OK\n";
    }

    std::cout << "\n=== Builder/Parser round-trip ===\n";

    {
        auto tid = make_tid(0x55);
        auto raw = message::make401(tid, "realm", "nonce");
        message::TurnMessage msg;
        assert(message::parse(raw.data(), raw.size(), msg) == message::ParseResult::Ok);
        assert(msg.transaction_id == tid);
        assert(msg.msg_class == message::MessageClass::ErrorResponse);
        std::cout << "[round-trip] make401 OK\n";
    }

    {
        auto tid = make_tid(0x66);
        auto raw = message::MessageBuilder(
                       message::Method::ChannelBind,
                       message::MessageClass::SuccessResponse, tid)
            .build();
        message::TurnMessage msg;
        assert(message::parse(raw.data(), raw.size(), msg) == message::ParseResult::Ok);
        assert(msg.method == message::Method::ChannelBind);
        std::cout << "[round-trip] ChannelBind method OK\n";
    }

    {
        auto tid = make_tid(0x77);
        auto raw = message::MessageBuilder(
                       message::Method::Refresh,
                       message::MessageClass::Request, tid)
            .addLifetime(0)
            .build();
        message::TurnMessage msg;
        assert(message::parse(raw.data(), raw.size(), msg) == message::ParseResult::Ok);
        assert(msg.method == message::Method::Refresh);
        auto lt = msg.findAttr(message::AttrType::Lifetime);
        assert(lt.has_value());
        uint32_t val = (static_cast<uint32_t>(lt->value[0]) << 24) |
                       (static_cast<uint32_t>(lt->value[1]) << 16) |
                       (static_cast<uint32_t>(lt->value[2]) <<  8) |
                        static_cast<uint32_t>(lt->value[3]);
        assert(val == 0);
        std::cout << "[round-trip] Refresh LIFETIME=0 OK\n";
    }

    std::cout << "\n=== Data indication ===\n";

    {
        auto tid = make_tid(0x88);
        std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04};
        auto raw = message::make_data_indication(tid, 0xC0000201, 12345, payload);

        message::TurnMessage msg;
        assert(message::parse(raw.data(), raw.size(), msg) == message::ParseResult::Ok);
        assert(msg.method    == message::Method::Send);
        assert(msg.msg_class == message::MessageClass::Indication);

        auto data_attr = msg.findAttr(message::AttrType::Data);
        assert(data_attr.has_value());
        assert(data_attr->value == payload);

        auto peer = msg.findAttr(message::AttrType::XorMappedAddress);
        assert(peer.has_value());
        std::cout << "[data indication] build/parse OK\n";
    }

    {
        auto tid = make_tid(0x89);
        std::vector<uint8_t> empty_payload;
        auto raw = message::make_data_indication(tid, 0xC0000201, 3478, empty_payload);
        message::TurnMessage msg;
        assert(message::parse(raw.data(), raw.size(), msg) == message::ParseResult::Ok);
        assert(msg.method == message::Method::Send);
        std::cout << "[data indication] empty payload OK\n";
    }

    std::cout << "\n=== ChannelData ===\n";

    {
        std::vector<uint8_t> payload = {0xDE, 0xAD, 0xBE, 0xEF};
        auto raw = message::make_channel_data(0x4001, payload);

        assert(raw.size() >= 4);
        assert((raw[0] & 0xC0) == 0x40);
        assert(message::is_channel_data(raw.data(), raw.size()));

        message::ChannelDataMessage ch;
        auto r = message::parse_channel_data(raw.data(), raw.size(), ch);
        assert(r == message::ChannelDataResult::Ok);
        assert(ch.channel_number == 0x4001);
        assert(ch.data == payload);
        std::cout << "[channel data] build/parse OK\n";
    }

    {
        std::vector<uint8_t> payload = {0x01, 0x02, 0x03};
        auto raw = message::make_channel_data(0x4FFF, payload);
        message::ChannelDataMessage ch;
        auto r = message::parse_channel_data(raw.data(), raw.size(), ch);
        assert(r == message::ChannelDataResult::Ok);
        assert(ch.channel_number == 0x4FFF);
        assert(ch.data == payload);
        assert(raw.size() % 4 == 0);
        std::cout << "[channel data] max channel + padding OK\n";
    }

    {
        std::vector<uint8_t> payload = {};
        auto raw = message::make_channel_data(0x4000, payload);
        message::ChannelDataMessage ch;
        auto r = message::parse_channel_data(raw.data(), raw.size(), ch);
        assert(r == message::ChannelDataResult::Ok);
        assert(ch.data.empty());
        std::cout << "[channel data] empty payload OK\n";
    }

    {
        std::vector<uint8_t> raw(3, 0x00);
        message::ChannelDataMessage ch;
        assert(message::parse_channel_data(raw.data(), raw.size(), ch)
               == message::ChannelDataResult::TooShort);
        std::cout << "[channel data] too short OK\n";
    }

    {
        std::vector<uint8_t> raw = {0x50, 0x00, 0x00, 0x00};
        message::ChannelDataMessage ch;
        assert(message::parse_channel_data(raw.data(), raw.size(), ch)
               == message::ChannelDataResult::InvalidChannel);
        std::cout << "[channel data] invalid channel (above 0x4FFF) OK\n";
    }

    {
        std::vector<uint8_t> raw = {0x3F, 0xFF, 0x00, 0x00};
        message::ChannelDataMessage ch;
        assert(message::parse_channel_data(raw.data(), raw.size(), ch)
               == message::ChannelDataResult::InvalidChannel);
        std::cout << "[channel data] invalid channel (below 0x4000) OK\n";
    }

    {
        assert(!message::is_channel_data(nullptr, 0));
        std::vector<uint8_t> stun = {0x00, 0x03};
        assert(!message::is_channel_data(stun.data(), stun.size()));
        std::vector<uint8_t> chan = {0x40, 0x01};
        assert( message::is_channel_data(chan.data(), chan.size()));
        std::cout << "[channel data] is_channel_data OK\n";
    }

    std::cout << "\nAll turn tests passed\n";
    return 0;
}