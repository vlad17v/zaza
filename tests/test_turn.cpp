#include "message/parser.hpp"
#include "message/builder.hpp"
#include "message/xor_codec.hpp"
#include "auth/nonce_manager.hpp"
#include "auth/hmac_validator.hpp"
#include "auth/long_term_cred.hpp"
#include "crypto/sha256.hpp"
#include "crypto/hmac.hpp"
#include "allocation/allocation.hpp"
#include "allocation/allocation_manager.hpp"
#include "allocation/permission_table.hpp"

#include <iostream>
#include <cassert>
#include <cstring>
#include <vector>
#include <array>
#include <thread>
#include <chrono>
#include <boost/asio/ssl.hpp>
#include <openssl/ssl.h>
#include <openssl/rand.h>
namespace ssl = boost::asio::ssl;

static std::array<uint8_t, 12> make_tid(uint8_t fill = 0xAB) {
    std::array<uint8_t, 12> tid;
    tid.fill(fill);
    return tid;
}

static std::vector<uint8_t> make_allocate_request(
    const std::array<uint8_t, 12>& tid)
{
    std::vector<uint8_t> msg(20);
    msg[0] = 0x00; msg[1] = 0x03;
    msg[2] = 0x00; msg[3] = 0x00;
    msg[4] = 0x21; msg[5] = 0x12; msg[6] = 0xA4; msg[7] = 0x42;
    std::copy(tid.begin(), tid.end(), msg.begin() + 8);
    return msg;
}

static std::vector<uint8_t> build_msg_with_attrs(
    message::Method       method,
    message::MessageClass cls,
    const std::array<uint8_t, 12>& tid,
    const std::vector<std::pair<message::AttrType,
                                std::vector<uint8_t>>>& attrs)
{
    message::MessageBuilder b(method, cls, tid);
    for (auto& [t, v] : attrs)
        b.addAttr(t, v);
    return b.build();
}

static std::vector<uint8_t> sign_message(
    std::vector<uint8_t>        raw,
    const std::string&          username,
    const std::string&          realm,
    const std::string&          password)
{
    auto key_bytes = crypto::long_term_key(username, realm, password);
    std::vector<uint8_t> key_vec(key_bytes.begin(), key_bytes.end());

    uint16_t attrs_len = (static_cast<uint16_t>(raw[2]) << 8) | raw[3];
    size_t off = 20;
    size_t mi_offset = 0;

    while (off + 4 <= 20u + attrs_len) {
        uint16_t t = (static_cast<uint16_t>(raw[off])   << 8) | raw[off+1];
        uint16_t l = (static_cast<uint16_t>(raw[off+2]) << 8) | raw[off+3];
        if (t == static_cast<uint16_t>(
                message::AttrType::MessageIntegritySha256)) {
            mi_offset = off;
            break;
        }
        off += 4 + l;
        if (l % 4 != 0) off += 4 - (l % 4);
    }

    uint16_t adj = static_cast<uint16_t>(mi_offset - 20 + 4 + 32);
    std::vector<uint8_t> buf(mi_offset + 4 + 32);
    std::copy(raw.begin(), raw.begin() + buf.size(), buf.begin());
    buf[2] = (adj >> 8) & 0xFF;
    buf[3] =  adj       & 0xFF;

    std::string s(buf.begin(), buf.end());
    auto mi = crypto::hmac_sha256(key_vec, s);
    std::copy(mi.begin(), mi.end(), raw.begin() + mi_offset + 4);
    return raw;
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
        assert(message::xor_port(0) ==
               static_cast<uint16_t>(message::kMagicCookie >> 16));
        assert(message::xor_addr_v4(0) == message::kMagicCookie);
        std::cout << "[xor] zero values OK\n";
    }

    std::cout << "\n=== Parser ===\n";

    {
        auto tid = make_tid();
        auto raw = make_allocate_request(tid);
        message::TurnMessage msg;
        assert(message::parse(raw.data(), raw.size(), msg)
               == message::ParseResult::Ok);
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
        message::TurnMessage msg;
        assert(message::parse(nullptr, 0, msg)
               == message::ParseResult::TooShort);
        std::cout << "[parser] empty OK\n";
    }

    {
        auto raw = make_allocate_request(make_tid());
        raw[0] = 0x80;
        message::TurnMessage msg;
        assert(message::parse(raw.data(), raw.size(), msg)
               == message::ParseResult::NotTurnMessage);
        std::cout << "[parser] not TURN message OK\n";
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
        raw[2] = 0x01; raw[3] = 0x00;
        message::TurnMessage msg;
        assert(message::parse(raw.data(), raw.size(), msg)
               == message::ParseResult::BadLength);
        std::cout << "[parser] bad length OK\n";
    }

    {
        auto tid = make_tid(0x01);
        std::vector<uint8_t> raw(28, 0x00);
        raw[0] = 0x00; raw[1] = 0x03;
        raw[2] = 0x00; raw[3] = 0x08;
        raw[4] = 0x21; raw[5] = 0x12; raw[6] = 0xA4; raw[7] = 0x42;
        std::copy(tid.begin(), tid.end(), raw.begin() + 8);
        raw[20] = 0x00; raw[21] = 0x19;
        raw[22] = 0x00; raw[23] = 0x04;
        raw[24] = 0x00; raw[25] = 0x00; raw[26] = 0x00; raw[27] = 0x11;
        message::TurnMessage msg;
        assert(message::parse(raw.data(), raw.size(), msg)
               == message::ParseResult::Ok);
        assert(msg.attributes.size() == 1);
        assert(msg.attributes[0].type ==
               message::AttrType::RequestedTransport);
        assert(msg.attributes[0].value[3] == 0x11);
        std::cout << "[parser] REQUESTED-TRANSPORT attr OK\n";
    }

    {
        auto raw = make_allocate_request(make_tid());
        message::TurnMessage msg;
        message::parse(raw.data(), raw.size(), msg);
        assert(!msg.findAttr(message::AttrType::Username).has_value());
        std::cout << "[parser] findAttr missing OK\n";
    }

    {
        auto tid = make_tid(0x02);
        std::vector<uint8_t> raw(28, 0x00);
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
        assert(message::parse(raw.data(), raw.size(), msg)
               == message::ParseResult::Ok);
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
        assert(message::parse(raw.data(), raw.size(), msg)
               == message::ParseResult::Ok);
        assert(msg.msg_class == message::MessageClass::ErrorResponse);
        auto err = msg.findAttr(message::AttrType::ErrorCode);
        assert(err.has_value());
        assert(err->value[2] == 4);
        assert(err->value[3] == 1);
        auto realm = msg.findAttr(message::AttrType::Realm);
        assert(realm.has_value());
        assert(std::string(realm->value.begin(), realm->value.end())
               == "chat.example.com");
        auto nonce = msg.findAttr(message::AttrType::Nonce);
        assert(nonce.has_value());
        assert(std::string(nonce->value.begin(), nonce->value.end())
               == "testnonce123");
        std::cout << "[builder] make401 OK\n";
    }

    {
        auto tid = make_tid(0x22);
        auto raw = message::make438(tid, "chat.example.com", "newnonce456");
        message::TurnMessage msg;
        assert(message::parse(raw.data(), raw.size(), msg)
               == message::ParseResult::Ok);
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
        assert(message::parse(raw.data(), raw.size(), msg)
               == message::ParseResult::Ok);
        assert(msg.msg_class == message::MessageClass::SuccessResponse);
        assert(msg.method    == message::Method::Allocate);
        auto lt = msg.findAttr(message::AttrType::Lifetime);
        assert(lt.has_value());
        uint32_t lifetime = (static_cast<uint32_t>(lt->value[0]) << 24) |
                            (static_cast<uint32_t>(lt->value[1]) << 16) |
                            (static_cast<uint32_t>(lt->value[2]) <<  8) |
                             static_cast<uint32_t>(lt->value[3]);
        assert(lifetime == 600);
        assert(msg.findAttr(message::AttrType::XorMappedAddress).has_value());
        assert(msg.findAttr(message::AttrType::XorRelayedAddress).has_value());
        std::cout << "[builder] success response with attrs OK\n";
    }

    {
        auto tid = make_tid(0x44);
        auto raw = message::MessageBuilder(
                       message::Method::Allocate,
                       message::MessageClass::SuccessResponse, tid)
            .addLifetime(600).build();
        message::TurnMessage msg;
        assert(message::parse(raw.data(), raw.size(), msg)
               == message::ParseResult::Ok);
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
        assert(message::parse(raw.data(), raw.size(), msg)
               == message::ParseResult::Ok);
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
        assert(message::parse(raw.data(), raw.size(), msg)
               == message::ParseResult::Ok);
        assert(msg.method == message::Method::ChannelBind);
        std::cout << "[round-trip] ChannelBind method OK\n";
    }

    {
        auto tid = make_tid(0x77);
        auto raw = message::MessageBuilder(
                       message::Method::Refresh,
                       message::MessageClass::Request, tid)
            .addLifetime(0).build();
        message::TurnMessage msg;
        assert(message::parse(raw.data(), raw.size(), msg)
               == message::ParseResult::Ok);
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
        std::array<uint8_t, 12> tid{};
        tid.fill(0xAB);
        uint32_t peer_ip   = 0x7F000001;
        uint16_t peer_port = 12345;
        std::vector<uint8_t> payload = {0x01, 0x02, 0x03};

        auto raw = message::make_data_indication(tid, peer_ip, peer_port, payload);

        message::TurnMessage msg;
        auto result = message::parse(raw.data(), raw.size(), msg);
        assert(result == message::ParseResult::Ok);
        assert(msg.method == message::Method::Data);
        assert(msg.msg_class == message::MessageClass::Indication);

        auto data_attr = msg.findAttr(message::AttrType::Data);
        assert(data_attr.has_value());
        assert(data_attr->value == payload);
        std::cout << "[data indication] build/parse OK\n";
    }

    {
        std::array<uint8_t, 12> tid{};
        std::vector<uint8_t> empty_payload;
        auto raw = message::make_data_indication(tid, 0x7F000001, 9999, empty_payload);
        message::TurnMessage msg;
        auto result = message::parse(raw.data(), raw.size(), msg);
        assert(result == message::ParseResult::Ok);
        assert(msg.method == message::Method::Data);
        std::cout << "[data indication] empty payload OK\n";
    }

    std::cout << "\n=== ChannelData ===\n";

    {
        std::vector<uint8_t> payload = {0xDE, 0xAD, 0xBE, 0xEF};
        auto raw = message::make_channel_data(0x4001, payload);
        assert((raw[0] & 0xC0) == 0x40);
        assert(message::is_channel_data(raw.data(), raw.size()));
        message::ChannelDataMessage ch;
        assert(message::parse_channel_data(raw.data(), raw.size(), ch)
               == message::ChannelDataResult::Ok);
        assert(ch.channel_number == 0x4001);
        assert(ch.data == payload);
        std::cout << "[channel data] build/parse OK\n";
    }

    {
        std::vector<uint8_t> payload = {0x01, 0x02, 0x03};
        auto raw = message::make_channel_data(0x4FFF, payload);
        message::ChannelDataMessage ch;
        assert(message::parse_channel_data(raw.data(), raw.size(), ch)
               == message::ChannelDataResult::Ok);
        assert(ch.channel_number == 0x4FFF);
        assert(ch.data == payload);
        assert(raw.size() % 4 == 0);
        std::cout << "[channel data] max channel + padding OK\n";
    }

    {
        auto raw = message::make_channel_data(0x4000, {});
        message::ChannelDataMessage ch;
        assert(message::parse_channel_data(raw.data(), raw.size(), ch)
               == message::ChannelDataResult::Ok);
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
        std::cout << "[channel data] invalid channel above 0x4FFF OK\n";
    }

    {
        std::vector<uint8_t> raw = {0x3F, 0xFF, 0x00, 0x00};
        message::ChannelDataMessage ch;
        assert(message::parse_channel_data(raw.data(), raw.size(), ch)
               == message::ChannelDataResult::InvalidChannel);
        std::cout << "[channel data] invalid channel below 0x4000 OK\n";
    }

    {
        std::vector<uint8_t> stun = {0x00, 0x03};
        assert(!message::is_channel_data(stun.data(), stun.size()));
        std::vector<uint8_t> chan = {0x40, 0x01};
        assert( message::is_channel_data(chan.data(), chan.size()));
        assert(!message::is_channel_data(nullptr, 0));
        std::cout << "[channel data] is_channel_data OK\n";
    }

    std::cout << "\n=== NonceManager ===\n";

    {
        turn_auth::NonceManager nm(std::chrono::seconds(3600));
        auto n1 = nm.generate();
        auto n2 = nm.generate();
        assert(!n1.empty());
        assert(!n2.empty());
        assert(n1 != n2);
        assert(n1.size() == 32);
        std::cout << "[nonce] generate unique OK\n";
    }

    {
        turn_auth::NonceManager nm(std::chrono::seconds(3600));
        auto n = nm.generate();
        assert(nm.isValid(n));
        std::cout << "[nonce] isValid OK\n";
    }

    {
        turn_auth::NonceManager nm(std::chrono::seconds(3600));
        assert(!nm.isValid("nonexistent_nonce"));
        std::cout << "[nonce] isValid nonexistent OK\n";
    }

    {
        turn_auth::NonceManager nm(std::chrono::seconds(3600));
        auto n = nm.generate();
        nm.invalidate(n);
        assert(!nm.isValid(n));
        std::cout << "[nonce] invalidate OK\n";
    }

    {
        turn_auth::NonceManager nm(std::chrono::seconds(3600));
        nm.invalidate("does_not_exist");
        std::cout << "[nonce] invalidate nonexistent OK\n";
    }

    {
        turn_auth::NonceManager nm(std::chrono::seconds(0));
        auto n = nm.generate();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        assert(!nm.isValid(n));
        std::cout << "[nonce] expired OK\n";
    }

    {
        turn_auth::NonceManager nm(std::chrono::seconds(3600));
        std::vector<std::string> nonces;
        for (int i = 0; i < 10; ++i)
            nonces.push_back(nm.generate());
        nm.cleanup();
        for (auto& n : nonces)
            assert(nm.isValid(n));
        std::cout << "[nonce] cleanup keeps valid OK\n";
    }

    {
        turn_auth::NonceManager nm(std::chrono::seconds(0));
        auto n = nm.generate();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        nm.cleanup();
        assert(!nm.isValid(n));
        std::cout << "[nonce] cleanup removes expired OK\n";
    }

    std::cout << "\n=== HmacValidator ===\n";

    {
        turn_auth::HmacValidator v("mysecret");
        auto creds = v.generate("user1", 3600);
        assert(!creds.username.empty());
        assert(!creds.password.empty());
        assert(creds.username.find("user1") != std::string::npos);
        assert(v.validate(creds.username, creds.password));
        std::cout << "[hmac_validator] generate/validate OK\n";
    }

    {
        turn_auth::HmacValidator v("mysecret");
        auto creds = v.generate("user1", -1);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        assert(!v.validate(creds.username, creds.password));
        std::cout << "[hmac_validator] expired credentials OK\n";
    }

    {
        turn_auth::HmacValidator v("mysecret");
        auto creds = v.generate("user1", 3600);
        assert(!v.validate(creds.username, "wrongpassword"));
        std::cout << "[hmac_validator] wrong password OK\n";
    }

    {
        turn_auth::HmacValidator v("mysecret");
        assert(!v.validate("no_colon_username", "anypass"));
        std::cout << "[hmac_validator] invalid username format OK\n";
    }

    {
        turn_auth::HmacValidator v("mysecret");
        assert(!v.validate("", ""));
        std::cout << "[hmac_validator] empty credentials OK\n";
    }

    {
        turn_auth::HmacValidator v1("secret_a");
        turn_auth::HmacValidator v2("secret_b");
        auto creds = v1.generate("user1", 3600);
        assert(!v2.validate(creds.username, creds.password));
        std::cout << "[hmac_validator] wrong secret OK\n";
    }

    {
        turn_auth::HmacValidator v("mysecret");
        auto creds = v.generate("user1", 3600);
        assert(v.getPassword(creds.username) == creds.password);
        std::cout << "[hmac_validator] getPassword OK\n";
    }

    {
        turn_auth::HmacValidator v("mysecret");
        assert(v.getPassword("no_colon").empty());
        std::cout << "[hmac_validator] getPassword invalid format OK\n";
    }

    {
        turn_auth::HmacValidator v("mysecret");
        assert(v.getPassword("").empty());
        std::cout << "[hmac_validator] getPassword empty OK\n";
    }

    {
        turn_auth::HmacValidator v("mysecret");
        auto c1 = v.generate("user1", 3600);
        auto c2 = v.generate("user2", 3600);
        assert(c1.username != c2.username);
        assert(c1.password != c2.password);
        assert( v.validate(c1.username, c1.password));
        assert( v.validate(c2.username, c2.password));
        assert(!v.validate(c1.username, c2.password));
        assert(!v.validate(c2.username, c1.password));
        std::cout << "[hmac_validator] different users independent OK\n";
    }

    std::cout << "\n=== LongTermCred ===\n";

    {
        turn_auth::NonceManager  nm(std::chrono::seconds(3600));
        turn_auth::HmacValidator hv("secret");
        turn_auth::LongTermCred  lc("chat.example.com", nm, hv);
        auto ch = lc.makeChallenge();
        assert(ch.realm == "chat.example.com");
        assert(!ch.nonce.empty());
        assert(nm.isValid(ch.nonce));
        std::cout << "[long_term_cred] makeChallenge OK\n";
    }

    {
        turn_auth::NonceManager  nm(std::chrono::seconds(3600));
        turn_auth::HmacValidator hv("secret");
        turn_auth::LongTermCred  lc("chat.example.com", nm, hv);
        auto ch1 = lc.makeChallenge();
        auto ch2 = lc.makeChallenge();
        assert(ch1.nonce != ch2.nonce);
        std::cout << "[long_term_cred] makeChallenge unique nonces OK\n";
    }

    {
        turn_auth::NonceManager  nm(std::chrono::seconds(3600));
        turn_auth::HmacValidator hv("secret");
        turn_auth::LongTermCred  lc("chat.example.com", nm, hv);
        message::TurnMessage msg;
        msg.method    = message::Method::Send;
        msg.msg_class = message::MessageClass::Indication;
        std::vector<uint8_t> raw(20, 0x00);
        assert(lc.authenticate(msg, raw.data(), raw.size(), "")
               == turn_auth::AuthResult::Ok);
        std::cout << "[long_term_cred] indication skips auth OK\n";
    }

    {
        turn_auth::NonceManager  nm(std::chrono::seconds(3600));
        turn_auth::HmacValidator hv("secret");
        turn_auth::LongTermCred  lc("chat.example.com", nm, hv);
        message::TurnMessage msg;
        msg.method    = message::Method::Allocate;
        msg.msg_class = message::MessageClass::Request;
        std::vector<uint8_t> raw(20, 0x00);
        assert(lc.authenticate(msg, raw.data(), raw.size(), "")
               == turn_auth::AuthResult::MissingCredentials);
        std::cout << "[long_term_cred] missing credentials OK\n";
    }

    {
        turn_auth::NonceManager  nm(std::chrono::seconds(3600));
        turn_auth::HmacValidator hv("secret");
        turn_auth::LongTermCred  lc("chat.example.com", nm, hv);
        std::string stale = "stale_nonce_not_in_manager";
        message::TurnMessage msg;
        msg.method    = message::Method::Allocate;
        msg.msg_class = message::MessageClass::Request;
        msg.attributes = {
            {message::AttrType::Username,
             std::vector<uint8_t>{'u',':','1'}},
            {message::AttrType::Realm,
             std::vector<uint8_t>{'r'}},
            {message::AttrType::Nonce,
             std::vector<uint8_t>(stale.begin(), stale.end())},
            {message::AttrType::MessageIntegritySha256,
             std::vector<uint8_t>(32, 0x00)},
        };
        std::vector<uint8_t> raw(20, 0x00);
        assert(lc.authenticate(msg, raw.data(), raw.size(), "")
               == turn_auth::AuthResult::StaleNonce);
        std::cout << "[long_term_cred] stale nonce OK\n";
    }

    {
        turn_auth::NonceManager  nm(std::chrono::seconds(3600));
        turn_auth::HmacValidator hv("secret");
        turn_auth::LongTermCred  lc("chat.example.com", nm, hv);
        auto creds = hv.generate("user1", -1);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto nonce = nm.generate();
        message::TurnMessage msg;
        msg.method    = message::Method::Allocate;
        msg.msg_class = message::MessageClass::Request;
        msg.attributes = {
            {message::AttrType::Username,
             std::vector<uint8_t>(creds.username.begin(), creds.username.end())},
            {message::AttrType::Realm,
             std::vector<uint8_t>{'r'}},
            {message::AttrType::Nonce,
             std::vector<uint8_t>(nonce.begin(), nonce.end())},
            {message::AttrType::MessageIntegritySha256,
             std::vector<uint8_t>(32, 0x00)},
        };
        std::vector<uint8_t> raw(20, 0x00);
        assert(lc.authenticate(msg, raw.data(), raw.size(), "")
               == turn_auth::AuthResult::CredentialsExpired);
        std::cout << "[long_term_cred] expired credentials OK\n";
    }

    {
        turn_auth::NonceManager  nm(std::chrono::seconds(3600));
        turn_auth::HmacValidator hv("secret");
        turn_auth::LongTermCred  lc("chat.example.com", nm, hv);
        auto creds = hv.generate("user1", 3600);
        auto nonce  = nm.generate();
        std::string realm    = "chat.example.com";
        std::string password = hv.getPassword(creds.username);
        std::vector<uint8_t> username_v(creds.username.begin(), creds.username.end());
        std::vector<uint8_t> realm_v(realm.begin(), realm.end());
        std::vector<uint8_t> nonce_v(nonce.begin(), nonce.end());
        auto raw = sign_message(
            message::MessageBuilder(
                message::Method::Allocate,
                message::MessageClass::Request, make_tid(0xCC))
            .addAttr(message::AttrType::Username,  username_v)
            .addAttr(message::AttrType::Realm,     realm_v)
            .addAttr(message::AttrType::Nonce,     nonce_v)
            .addAttr(message::AttrType::MessageIntegritySha256,
                     std::vector<uint8_t>(32, 0x00))
            .build(),
            creds.username, realm, password);
        message::TurnMessage msg;
        assert(message::parse(raw.data(), raw.size(), msg)
               == message::ParseResult::Ok);
        assert(lc.authenticate(msg, raw.data(), raw.size(), "")
               == turn_auth::AuthResult::Ok);
        std::cout << "[long_term_cred] correct MESSAGE-INTEGRITY-SHA256 OK\n";
    }

    {
        turn_auth::NonceManager  nm(std::chrono::seconds(3600));
        turn_auth::HmacValidator hv("secret");
        turn_auth::LongTermCred  lc("chat.example.com", nm, hv);
        auto creds = hv.generate("user1", 3600);
        auto nonce  = nm.generate();
        std::string realm    = "chat.example.com";
        std::string password = hv.getPassword(creds.username);
        std::vector<uint8_t> username_v(creds.username.begin(), creds.username.end());
        std::vector<uint8_t> realm_v(realm.begin(), realm.end());
        std::vector<uint8_t> nonce_v(nonce.begin(), nonce.end());
        auto raw = sign_message(
            message::MessageBuilder(
                message::Method::Refresh,
                message::MessageClass::Request, make_tid(0xBB))
            .addAttr(message::AttrType::Username,  username_v)
            .addAttr(message::AttrType::Realm,     realm_v)
            .addAttr(message::AttrType::Nonce,     nonce_v)
            .addAttr(message::AttrType::MessageIntegritySha256,
                     std::vector<uint8_t>(32, 0x00))
            .build(),
            creds.username, realm, password);
        message::TurnMessage msg;
        assert(message::parse(raw.data(), raw.size(), msg)
               == message::ParseResult::Ok);
        assert(lc.authenticate(msg, raw.data(), raw.size(), "other_user")
               == turn_auth::AuthResult::WrongCredentials);
        std::cout << "[long_term_cred] wrong credentials non-Allocate OK\n";
    }

    {
        turn_auth::NonceManager  nm(std::chrono::seconds(3600));
        turn_auth::HmacValidator hv("secret");
        turn_auth::LongTermCred  lc("chat.example.com", nm, hv);
        auto creds = hv.generate("user1", 3600);
        auto nonce  = nm.generate();
        std::vector<uint8_t> username_v(creds.username.begin(), creds.username.end());
        std::vector<uint8_t> nonce_v(nonce.begin(), nonce.end());
        auto raw = message::MessageBuilder(
                       message::Method::Allocate,
                       message::MessageClass::Request, make_tid(0xDD))
            .addAttr(message::AttrType::Username,  username_v)
            .addAttr(message::AttrType::Realm,
                     std::vector<uint8_t>{'r'})
            .addAttr(message::AttrType::Nonce,     nonce_v)
            .addAttr(message::AttrType::MessageIntegritySha256,
                     std::vector<uint8_t>(32, 0x00))
            .build();
        message::TurnMessage msg;
        assert(message::parse(raw.data(), raw.size(), msg)
               == message::ParseResult::Ok);
        assert(lc.authenticate(msg, raw.data(), raw.size(), "")
               == turn_auth::AuthResult::BadIntegrity);
        std::cout << "[long_term_cred] bad integrity (wrong MI) OK\n";
    }

    {
        turn_auth::NonceManager  nm(std::chrono::seconds(3600));
        turn_auth::HmacValidator hv("secret");
        turn_auth::LongTermCred  lc("chat.example.com", nm, hv);
        auto creds = hv.generate("user1", 3600);
        auto nonce  = nm.generate();
        message::TurnMessage msg;
        msg.method    = message::Method::Allocate;
        msg.msg_class = message::MessageClass::Request;
        msg.attributes = {
            {message::AttrType::Username,
             std::vector<uint8_t>(creds.username.begin(), creds.username.end())},
            {message::AttrType::Realm,
             std::vector<uint8_t>{'r'}},
            {message::AttrType::Nonce,
             std::vector<uint8_t>(nonce.begin(), nonce.end())},
            {message::AttrType::MessageIntegritySha256,
             std::vector<uint8_t>(32, 0x00)},
        };
        std::vector<uint8_t> raw(20, 0x00);
        assert(lc.authenticate(msg, raw.data(), raw.size(), "")
               == turn_auth::AuthResult::BadIntegrity);
        std::cout << "[long_term_cred] bad integrity (wrong MI) OK\n";
    }

    {
        turn_auth::NonceManager  nm(std::chrono::seconds(3600));
        turn_auth::HmacValidator hv("secret");
        turn_auth::LongTermCred  lc("chat.example.com", nm, hv);
        auto creds = hv.generate("user1", 3600);
        auto nonce  = nm.generate();
        std::string realm = "chat.example.com";
        std::string password = hv.getPassword(creds.username);
        auto key_bytes = crypto::long_term_key(
            creds.username, realm, password);
        std::vector<uint8_t> key_vec(key_bytes.begin(), key_bytes.end());
        std::vector<uint8_t> username_v(creds.username.begin(),
                                        creds.username.end());
        std::vector<uint8_t> realm_v(realm.begin(), realm.end());
        std::vector<uint8_t> nonce_v(nonce.begin(), nonce.end());
        std::vector<uint8_t> mi_placeholder(32, 0x00);
        auto raw = message::MessageBuilder(
                       message::Method::Allocate,
                       message::MessageClass::Request, make_tid(0xAA))
            .addAttr(message::AttrType::Username,  username_v)
            .addAttr(message::AttrType::Realm,     realm_v)
            .addAttr(message::AttrType::Nonce,     nonce_v)
            .addAttr(message::AttrType::MessageIntegritySha256, mi_placeholder)
            .build();
        uint16_t mi_offset = 0;
        uint16_t attrs_len = (static_cast<uint16_t>(raw[2]) << 8) | raw[3];
        size_t off = 20;
        while (off + 4 <= 20u + attrs_len) {
            uint16_t t = (static_cast<uint16_t>(raw[off])   << 8) | raw[off+1];
            uint16_t l = (static_cast<uint16_t>(raw[off+2]) << 8) | raw[off+3];
            if (t == static_cast<uint16_t>(
                    message::AttrType::MessageIntegritySha256)) {
                mi_offset = static_cast<uint16_t>(off);
                break;
            }
            off += 4 + l;
            if (l % 4 != 0) off += 4 - (l % 4);
        }
        uint16_t adj = static_cast<uint16_t>(mi_offset - 20 + 4 + 32);
        std::vector<uint8_t> buf_for_hmac(mi_offset + 4 + 32);
        std::copy(raw.begin(), raw.begin() + buf_for_hmac.size(),
                  buf_for_hmac.begin());
        buf_for_hmac[2] = (adj >> 8) & 0xFF;
        buf_for_hmac[3] =  adj       & 0xFF;
        std::string s(buf_for_hmac.begin(), buf_for_hmac.end());
        auto mi = crypto::hmac_sha256(key_vec, s);
        std::copy(mi.begin(), mi.end(), raw.begin() + mi_offset + 4);
        message::TurnMessage msg;
        assert(message::parse(raw.data(), raw.size(), msg)
               == message::ParseResult::Ok);
        assert(lc.authenticate(msg, raw.data(), raw.size(), "")
               == turn_auth::AuthResult::Ok);
        std::cout << "[long_term_cred] correct MESSAGE-INTEGRITY-SHA256 OK\n";
    }

    std::cout << "\n=== isDeniedAddress ===\n";
    {
        assert( allocation::isDeniedAddress("10.0.0.1"));
        assert( allocation::isDeniedAddress("10.255.255.255"));
        assert( allocation::isDeniedAddress("172.16.0.1"));
        assert( allocation::isDeniedAddress("172.31.255.255"));
        assert(!allocation::isDeniedAddress("192.168.1.1"));
        assert(!allocation::isDeniedAddress("127.0.0.1"));
        assert(!allocation::isDeniedAddress("8.8.8.8"));
        assert(!allocation::isDeniedAddress("1.1.1.1"));
        std::cout << "[denied_address] RFC 1918 + loopback OK\n";
    }

    std::cout << "\n=== AllocationManager ===\n";

    {
        asio::io_context ioc;
        allocation::AllocationManager mgr(ioc, "127.0.0.1", 49152, 65535);

        transport::Endpoint client{"192.0.2.1", 54321};
        std::array<uint8_t, 12> tid; tid.fill(0x01);

        auto raw = message::MessageBuilder(
                       message::Method::Allocate,
                       message::MessageClass::Request, tid)
            .addAttr(message::AttrType::RequestedTransport,
                     {17, 0, 0, 0})
            .build();

        message::TurnMessage msg;
        message::parse(raw.data(), raw.size(), msg);

        auto resp = mgr.handleAllocate(msg, client, "user1", "realm");
        message::TurnMessage resp_msg;
        assert(message::parse(resp.data(), resp.size(), resp_msg)
               == message::ParseResult::Ok);
        assert(resp_msg.msg_class == message::MessageClass::SuccessResponse);
        assert(resp_msg.findAttr(message::AttrType::XorRelayedAddress).has_value());
        assert(resp_msg.findAttr(message::AttrType::XorMappedAddress).has_value());
        assert(resp_msg.findAttr(message::AttrType::Lifetime).has_value());
        std::cout << "[alloc] handle_allocate success OK\n";
    }

    {
        asio::io_context ioc;
        allocation::AllocationManager mgr(ioc, "127.0.0.1", 49152, 65535);
        transport::Endpoint client{"192.0.2.1", 54321};
        std::array<uint8_t, 12> tid; tid.fill(0x02);

        auto raw = message::MessageBuilder(
                       message::Method::Allocate,
                       message::MessageClass::Request, tid)
            .addAttr(message::AttrType::RequestedTransport, {17, 0, 0, 0})
            .build();
        message::TurnMessage msg;
        message::parse(raw.data(), raw.size(), msg);
        mgr.handleAllocate(msg, client, "user1", "realm");

        tid.fill(0x03);
        auto raw2 = message::MessageBuilder(
                        message::Method::Allocate,
                        message::MessageClass::Request, tid)
            .addAttr(message::AttrType::RequestedTransport, {17, 0, 0, 0})
            .build();
        message::TurnMessage msg2;
        message::parse(raw2.data(), raw2.size(), msg2);
        auto resp = mgr.handleAllocate(msg2, client, "user1", "realm");

        message::TurnMessage resp_msg;
        message::parse(resp.data(), resp.size(), resp_msg);
        assert(resp_msg.msg_class == message::MessageClass::ErrorResponse);
        auto err = resp_msg.findAttr(message::AttrType::ErrorCode);
        assert(err.has_value());
        assert(err->value[2] == 4 && err->value[3] == 37);
        std::cout << "[alloc] duplicate 5-tuple → 437 OK\n";
    }

    {
        asio::io_context ioc;
        allocation::AllocationManager mgr(ioc, "127.0.0.1", 49152, 65535);
        transport::Endpoint client{"192.0.2.2", 12345};
        std::array<uint8_t, 12> tid; tid.fill(0x04);

        auto raw = message::MessageBuilder(
                       message::Method::Allocate,
                       message::MessageClass::Request, tid)
            .build();
        message::TurnMessage msg;
        message::parse(raw.data(), raw.size(), msg);
        auto resp = mgr.handleAllocate(msg, client, "user1", "realm");

        message::TurnMessage resp_msg;
        message::parse(resp.data(), resp.size(), resp_msg);
        assert(resp_msg.msg_class == message::MessageClass::ErrorResponse);
        auto err = resp_msg.findAttr(message::AttrType::ErrorCode);
        assert(err.has_value());
        assert(err->value[2] == 4 && err->value[3] == 0);
        std::cout << "[alloc] missing REQUESTED-TRANSPORT → 400 OK\n";
    }

    {
        asio::io_context ioc;
        allocation::AllocationManager mgr(ioc, "127.0.0.1", 49152, 65535);
        transport::Endpoint client{"192.0.2.3", 11111};
        std::array<uint8_t, 12> tid; tid.fill(0x05);

        auto raw = message::MessageBuilder(
                       message::Method::Allocate,
                       message::MessageClass::Request, tid)
            .addAttr(message::AttrType::RequestedTransport, {17, 0, 0, 0})
            .build();
        message::TurnMessage msg;
        message::parse(raw.data(), raw.size(), msg);
        mgr.handleAllocate(msg, client, "user1", "realm");

        tid.fill(0x06);
        auto raw2 = message::MessageBuilder(
                        message::Method::Refresh,
                        message::MessageClass::Request, tid)
            .addLifetime(1200)
            .build();
        message::TurnMessage msg2;
        message::parse(raw2.data(), raw2.size(), msg2);
        auto resp = mgr.handleRefresh(msg2, client, "user1");

        message::TurnMessage resp_msg;
        message::parse(resp.data(), resp.size(), resp_msg);
        assert(resp_msg.msg_class == message::MessageClass::SuccessResponse);
        auto lt = resp_msg.findAttr(message::AttrType::Lifetime);
        assert(lt.has_value());
        uint32_t lifetime =
            (static_cast<uint32_t>(lt->value[0]) << 24) |
            (static_cast<uint32_t>(lt->value[1]) << 16) |
            (static_cast<uint32_t>(lt->value[2]) <<  8) |
             static_cast<uint32_t>(lt->value[3]);
        assert(lifetime == 1200);
        std::cout << "[alloc] handle_refresh update OK\n";
    }

    {
        asio::io_context ioc;
        allocation::AllocationManager mgr(ioc, "127.0.0.1", 49152, 65535);
        transport::Endpoint client{"192.0.2.4", 22222};
        std::array<uint8_t, 12> tid; tid.fill(0x07);

        auto raw = message::MessageBuilder(
                       message::Method::Allocate,
                       message::MessageClass::Request, tid)
            .addAttr(message::AttrType::RequestedTransport, {17, 0, 0, 0})
            .build();
        message::TurnMessage msg;
        message::parse(raw.data(), raw.size(), msg);
        mgr.handleAllocate(msg, client, "user1", "realm");
        assert(mgr.findByClient(client) != nullptr);

        tid.fill(0x08);
        auto raw2 = message::MessageBuilder(
                        message::Method::Refresh,
                        message::MessageClass::Request, tid)
            .addLifetime(0)
            .build();
        message::TurnMessage msg2;
        message::parse(raw2.data(), raw2.size(), msg2);
        auto resp = mgr.handleRefresh(msg2, client, "user1");

        message::TurnMessage resp_msg;
        message::parse(resp.data(), resp.size(), resp_msg);
        assert(resp_msg.msg_class == message::MessageClass::SuccessResponse);
        assert(mgr.findByClient(client) == nullptr);
        std::cout << "[alloc] handle_refresh delete (LIFETIME=0) OK\n";
    }

    {
        asio::io_context ioc;
        allocation::AllocationManager mgr(ioc, "127.0.0.1", 49152, 65535);
        transport::Endpoint client{"192.0.2.5", 33333};
        std::array<uint8_t, 12> tid; tid.fill(0x09);

        auto raw = message::MessageBuilder(
                       message::Method::Allocate,
                       message::MessageClass::Request, tid)
            .addAttr(message::AttrType::RequestedTransport, {17, 0, 0, 0})
            .build();
        message::TurnMessage msg;
        message::parse(raw.data(), raw.size(), msg);
        mgr.handleAllocate(msg, client, "user1", "realm");

        uint32_t peer_ip_xor = 0x08080808 ^ message::kMagicCookie;
        uint16_t peer_port_xor = 12345 ^ static_cast<uint16_t>(message::kMagicCookie >> 16);
        std::vector<uint8_t> peer_addr_attr = {
            0x00, 0x01,
            static_cast<uint8_t>(peer_port_xor >> 8),
            static_cast<uint8_t>(peer_port_xor & 0xFF),
            static_cast<uint8_t>(peer_ip_xor >> 24),
            static_cast<uint8_t>((peer_ip_xor >> 16) & 0xFF),
            static_cast<uint8_t>((peer_ip_xor >>  8) & 0xFF),
            static_cast<uint8_t>(peer_ip_xor & 0xFF),
        };

        tid.fill(0x0A);
        auto raw2 = message::MessageBuilder(
                        message::Method::CreatePermission,
                        message::MessageClass::Request, tid)
            .addAttr(message::AttrType::XorPeerAddress, peer_addr_attr)
            .build();
        message::TurnMessage msg2;
        message::parse(raw2.data(), raw2.size(), msg2);
        auto resp = mgr.handleCreatePermission(msg2, client);

        message::TurnMessage resp_msg;
        message::parse(resp.data(), resp.size(), resp_msg);
        assert(resp_msg.msg_class == message::MessageClass::SuccessResponse);

        auto alloc = mgr.findByClient(client);
        assert(alloc != nullptr);
        assert(alloc->hasPermission("8.8.8.8"));
        std::cout << "[alloc] handle_create_permission OK\n";
    }

    {
        asio::io_context ioc;
        allocation::AllocationManager mgr(ioc, "127.0.0.1", 49152, 65535);
        transport::Endpoint client{"192.0.2.6", 44444};
        std::array<uint8_t, 12> tid; tid.fill(0x0B);

        auto raw = message::MessageBuilder(
                       message::Method::Allocate,
                       message::MessageClass::Request, tid)
            .addAttr(message::AttrType::RequestedTransport, {17, 0, 0, 0})
            .build();
        message::TurnMessage msg;
        message::parse(raw.data(), raw.size(), msg);
        mgr.handleAllocate(msg, client, "user1", "realm");

        uint32_t peer_ip_xor = 0x0A000001 ^ message::kMagicCookie;
        uint16_t peer_port_xor = 80 ^ static_cast<uint16_t>(message::kMagicCookie >> 16);
        std::vector<uint8_t> peer_addr_attr = {
            0x00, 0x01,
            static_cast<uint8_t>(peer_port_xor >> 8),
            static_cast<uint8_t>(peer_port_xor & 0xFF),
            static_cast<uint8_t>(peer_ip_xor >> 24),
            static_cast<uint8_t>((peer_ip_xor >> 16) & 0xFF),
            static_cast<uint8_t>((peer_ip_xor >>  8) & 0xFF),
            static_cast<uint8_t>(peer_ip_xor & 0xFF),
        };

        tid.fill(0x0C);
        auto raw2 = message::MessageBuilder(
                        message::Method::CreatePermission,
                        message::MessageClass::Request, tid)
            .addAttr(message::AttrType::XorPeerAddress, peer_addr_attr)
            .build();
        message::TurnMessage msg2;
        message::parse(raw2.data(), raw2.size(), msg2);
        auto resp = mgr.handleCreatePermission(msg2, client);

        message::TurnMessage resp_msg;
        message::parse(resp.data(), resp.size(), resp_msg);
        assert(resp_msg.msg_class == message::MessageClass::ErrorResponse);
        auto err = resp_msg.findAttr(message::AttrType::ErrorCode);
        assert(err.has_value());
        assert(err->value[2] == 4 && err->value[3] == 3);
        std::cout << "[alloc] create_permission RFC 1918 → 403 OK\n";
    }

    {
        asio::io_context ioc;
        allocation::AllocationManager mgr(ioc, "127.0.0.1", 49152, 65535);
        transport::Endpoint client{"192.0.2.7", 55555};
        std::array<uint8_t, 12> tid; tid.fill(0x0D);

        auto raw = message::MessageBuilder(
                       message::Method::Allocate,
                       message::MessageClass::Request, tid)
            .addAttr(message::AttrType::RequestedTransport, {17, 0, 0, 0})
            .build();
        message::TurnMessage msg;
        message::parse(raw.data(), raw.size(), msg);
        mgr.handleAllocate(msg, client, "user1", "realm");

        uint32_t peer_ip_xor   = 0x08080808 ^ message::kMagicCookie;
        uint16_t peer_port_xor = 9000 ^ static_cast<uint16_t>(message::kMagicCookie >> 16);
        std::vector<uint8_t> peer_addr_attr = {
            0x00, 0x01,
            static_cast<uint8_t>(peer_port_xor >> 8),
            static_cast<uint8_t>(peer_port_xor & 0xFF),
            static_cast<uint8_t>(peer_ip_xor >> 24),
            static_cast<uint8_t>((peer_ip_xor >> 16) & 0xFF),
            static_cast<uint8_t>((peer_ip_xor >>  8) & 0xFF),
            static_cast<uint8_t>(peer_ip_xor & 0xFF),
        };

        uint16_t ch_num = 0x4001;
        uint16_t ch_num_xor = 0;
        std::vector<uint8_t> ch_attr = {
            static_cast<uint8_t>(ch_num >> 8),
            static_cast<uint8_t>(ch_num & 0xFF),
            0x00, 0x00
        };
        (void)ch_num_xor;

        tid.fill(0x0E);
        auto raw2 = message::MessageBuilder(
                        message::Method::ChannelBind,
                        message::MessageClass::Request, tid)
            .addAttr(message::AttrType::ChannelNumber,   ch_attr)
            .addAttr(message::AttrType::XorPeerAddress,  peer_addr_attr)
            .build();
        message::TurnMessage msg2;
        message::parse(raw2.data(), raw2.size(), msg2);
        auto resp = mgr.handleChannelBind(msg2, client);

        message::TurnMessage resp_msg;
        message::parse(resp.data(), resp.size(), resp_msg);
        assert(resp_msg.msg_class == message::MessageClass::SuccessResponse);

        auto alloc = mgr.findByClient(client);
        assert(alloc != nullptr);
        assert(alloc->findChannelByNumber(0x4001).has_value());
        assert(alloc->hasPermission("8.8.8.8"));
        std::cout << "[alloc] handle_channel_bind OK\n";
    }

    {
        asio::io_context ioc;
        allocation::AllocationManager mgr(ioc, "127.0.0.1", 49152, 65535);
        transport::Endpoint client{"192.0.2.8", 60000};
        std::array<uint8_t, 12> tid; tid.fill(0x0F);

        auto raw = message::MessageBuilder(
                       message::Method::Allocate,
                       message::MessageClass::Request, tid)
            .addAttr(message::AttrType::RequestedTransport, {17, 0, 0, 0})
            .build();
        message::TurnMessage msg;
        message::parse(raw.data(), raw.size(), msg);
        mgr.handleAllocate(msg, client, "user1", "realm");

        std::vector<uint8_t> received_data;
        transport::Endpoint  received_from;
        mgr.setClientSendCallback([&](const std::vector<uint8_t>& data,
                                      const transport::Endpoint&  to) {
            received_data = data;
            received_from = to;
        });

        auto alloc = mgr.findByClient(client);
        assert(alloc != nullptr);

        allocation::Permission perm;
        perm.address   = "8.8.8.8";
        perm.expiresAt = allocation::Clock::now() +
                         std::chrono::seconds(300);
        alloc->permissions["8.8.8.8"] = perm;

        transport::Endpoint peer{"8.8.8.8", 12345};
        std::vector<uint8_t> payload = {0x01, 0x02, 0x03};
        mgr.handlePeerData(payload.data(), payload.size(),
                           peer, alloc->relayedAddr);

        assert(!received_data.empty());
        assert(received_from.address == client.address);
        assert(received_from.port    == client.port);
        std::cout << "[alloc] handlePeerData → Data indication OK\n";
    }

    {
        asio::io_context ioc;
        allocation::AllocationManager mgr(ioc, "127.0.0.1", 49152, 65535);
        transport::Endpoint client{"192.0.2.9", 61000};
        std::array<uint8_t, 12> tid; tid.fill(0x10);

        auto raw = message::MessageBuilder(
                       message::Method::Allocate,
                       message::MessageClass::Request, tid)
            .addAttr(message::AttrType::RequestedTransport, {17, 0, 0, 0})
            .build();
        message::TurnMessage msg;
        message::parse(raw.data(), raw.size(), msg);
        mgr.handleAllocate(msg, client, "user1", "realm");

        auto alloc = mgr.findByClient(client);

        allocation::Permission perm;
        perm.address   = "8.8.8.8";
        perm.expiresAt = allocation::Clock::now() +
                         std::chrono::seconds(300);
        alloc->permissions["8.8.8.8"] = perm;

        allocation::ChannelBinding ch;
        ch.channelNumber = 0x4002;
        ch.peer          = {"8.8.8.8", 12345};
        ch.expiresAt     = allocation::Clock::now() +
                           std::chrono::seconds(600);
        alloc->channels[0x4002] = ch;

        std::vector<uint8_t> received_data;
        mgr.setClientSendCallback([&](const std::vector<uint8_t>& data,
                                      const transport::Endpoint&) {
            received_data = data;
        });

        transport::Endpoint peer{"8.8.8.8", 12345};
        std::vector<uint8_t> payload = {0xDE, 0xAD};
        mgr.handlePeerData(payload.data(), payload.size(),
                           peer, alloc->relayedAddr);

        assert(!received_data.empty());
        assert((received_data[0] & 0xC0) == 0x40);
        uint16_t ch_num = (static_cast<uint16_t>(received_data[0]) << 8) |
                           received_data[1];
        assert(ch_num == 0x4002);
        std::cout << "[alloc] handlePeerData → ChannelData OK\n";
    }

    {
        asio::io_context ioc;
        allocation::AllocationManager mgr(ioc, "127.0.0.1", 49152, 65535);
        transport::Endpoint client{"192.0.2.10", 62000};

        bool called = false;
        mgr.setClientSendCallback([&](const std::vector<uint8_t>&,
                                      const transport::Endpoint&) {
            called = true;
        });

        transport::Endpoint peer{"8.8.8.8", 9999};
        transport::Endpoint relay{"127.0.0.1", 50000};
        std::vector<uint8_t> data = {0x01};
        mgr.handlePeerData(data.data(), data.size(), peer, relay);
        assert(!called);
        std::cout << "[alloc] handlePeerData no allocation → silent drop OK\n";
    }

    {
        asio::io_context ioc;
        allocation::AllocationManager mgr(ioc, "127.0.0.1", 49152, 65535);
        transport::Endpoint client{"192.0.2.11", 63000};
        std::array<uint8_t, 12> tid; tid.fill(0x11);

        auto raw = message::MessageBuilder(
                       message::Method::Allocate,
                       message::MessageClass::Request, tid)
            .addAttr(message::AttrType::RequestedTransport, {17, 0, 0, 0})
            .build();
        message::TurnMessage msg;
        message::parse(raw.data(), raw.size(), msg);
        mgr.handleAllocate(msg, client, "user1", "realm");

        auto alloc = mgr.findByClient(client);

        bool called = false;
        mgr.setClientSendCallback([&](const std::vector<uint8_t>&,
                                      const transport::Endpoint&) {
            called = true;
        });

        transport::Endpoint peer{"8.8.8.8", 9999};
        std::vector<uint8_t> data = {0x01};
        mgr.handlePeerData(data.data(), data.size(),
                           peer, alloc->relayedAddr);
        assert(!called);
        std::cout << "[alloc] handlePeerData no permission → silent drop OK\n";
    }

    std::cout << "\n=== SSL context factory ===\n";

    {
        ssl::context tls_ctx(ssl::context::tls_server);
        tls_ctx.set_options(
            ssl::context::default_workarounds |
            ssl::context::no_sslv2           |
            ssl::context::no_sslv3           |
            ssl::context::no_tlsv1           |
            ssl::context::no_tlsv1_1);
        std::cout << "[ssl_ctx] TLS context created OK\n";
    }

    {
        SSL_CTX* ctx = SSL_CTX_new(DTLS_server_method());
        assert(ctx != nullptr);
        SSL_CTX_set_cookie_generate_cb(ctx,
            [](SSL*, uint8_t* cookie, uint32_t* len) -> int {
                *len = 16;
                RAND_bytes(cookie, 16);
                return 1;
            });
        SSL_CTX_set_cookie_verify_cb(ctx,
            [](SSL*, const uint8_t*, uint32_t len) -> int {
                return len == 16 ? 1 : 0;
            });
        SSL_CTX_free(ctx);
        std::cout << "[ssl_ctx] DTLS context created OK\n";
    }

    std::cout << "\nAll turn tests passed\n";
    return 0;
}