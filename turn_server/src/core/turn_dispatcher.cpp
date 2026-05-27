#include "turn_dispatcher.hpp"
#include "crypto/sha256.hpp"
#include "crypto/hmac.hpp"
#include "log/logger.hpp"

#include <sstream>

TurnDispatcher::TurnDispatcher(asio::io_context&  ioc,
                                const std::string& realm,
                                const std::string& shared_secret,
                                const std::string& relay_address,
                                uint16_t           relay_port_min,
                                uint16_t           relay_port_max)
    : nonce_manager_(std::chrono::seconds(3600))
    , hmac_validator_(shared_secret)
    , long_term_cred_(realm, nonce_manager_, hmac_validator_)
    , alloc_manager_(ioc, relay_address, relay_port_min, relay_port_max)
{
    alloc_manager_.setClientSendCallback(
        [this](const std::vector<uint8_t>& data,
               const transport::Endpoint&  to) {
            if (current_transport_)
                current_transport_->send(data, to);
        });

    alloc_manager_.setPeerSendCallback(
        [this](const std::vector<uint8_t>& data,
               const transport::Endpoint&  to) {
            if (current_transport_)
                current_transport_->send(data, to);
        });
}

void TurnDispatcher::send(transport::ITransport&      transport,
                           const transport::Endpoint&  to,
                           const std::vector<uint8_t>& data) {
    transport.send(data, to);
}

void TurnDispatcher::onPacket(const uint8_t*             data,
                               size_t                     size,
                               const transport::Endpoint& from,
                               transport::ITransport&     transport) {
    if (size == 0) return;

    if ((data[0] & 0xC0) == 0x40) {
        handleChannelData(data, size, from, transport);
        return;
    }

    if ((data[0] & 0xC0) == 0x00) {
        message::TurnMessage msg;
        auto result = message::parse(data, size, msg);
        if (result != message::ParseResult::Ok) {
            LOGE("[turn] parse failed from " + from.address + ":" +
                 std::to_string(from.port));
            return;
        }
        handleStun(msg, data, size, from, transport);
        return;
    }
}

void TurnDispatcher::handleStun(const message::TurnMessage& msg,
                                 const uint8_t*              raw,
                                 size_t                      raw_len,
                                 const transport::Endpoint&  from,
                                 transport::ITransport&      transport) {
    current_transport_ = &transport;

    if (msg.msg_class == message::MessageClass::Indication) {
        if (msg.method == message::Method::Send)
            alloc_manager_.handleSend(msg, from);
        return;
    }

    std::string allocated_username;
    if (msg.method != message::Method::Allocate) {
        auto alloc = alloc_manager_.findByClient(from);
        if (alloc) allocated_username = alloc->username;
    }

    auto auth_result = long_term_cred_.authenticate(
        msg, raw, raw_len, allocated_username);

    switch (auth_result) {
        case turn_auth::AuthResult::MissingCredentials: {
            auto ch = long_term_cred_.makeChallenge();
            std::vector<uint8_t> realm_v(ch.realm.begin(), ch.realm.end());
            std::vector<uint8_t> nonce_v(ch.nonce.begin(), ch.nonce.end());
            auto resp = message::MessageBuilder(
                            msg.method,
                            message::MessageClass::ErrorResponse,
                            msg.transaction_id)
                .addErrorCode(401, "Unauthorized")
                .addAttr(message::AttrType::Realm, realm_v)
                .addAttr(message::AttrType::Nonce, nonce_v)
                .build();
            send(transport, from, resp);
            return;
        }
        case turn_auth::AuthResult::StaleNonce: {
            auto ch = long_term_cred_.makeChallenge();
            std::vector<uint8_t> realm_v(ch.realm.begin(), ch.realm.end());
            std::vector<uint8_t> nonce_v(ch.nonce.begin(), ch.nonce.end());
            auto resp = message::MessageBuilder(
                            msg.method,
                            message::MessageClass::ErrorResponse,
                            msg.transaction_id)
                .addErrorCode(438, "Stale Nonce")
                .addAttr(message::AttrType::Realm, realm_v)
                .addAttr(message::AttrType::Nonce, nonce_v)
                .build();
            send(transport, from, resp);
            return;
        }
        case turn_auth::AuthResult::BadIntegrity:
        case turn_auth::AuthResult::CredentialsExpired: {
            auto ch = long_term_cred_.makeChallenge();
            std::vector<uint8_t> realm_v(ch.realm.begin(), ch.realm.end());
            std::vector<uint8_t> nonce_v(ch.nonce.begin(), ch.nonce.end());
            auto resp = message::MessageBuilder(
                            msg.method,
                            message::MessageClass::ErrorResponse,
                            msg.transaction_id)
                .addErrorCode(401, "Unauthorized")
                .addAttr(message::AttrType::Realm, realm_v)
                .addAttr(message::AttrType::Nonce, nonce_v)
                .build();
            LOGE("[turn] auth failed (bad integrity/expired)"
                 " from " + from.address + ":" + std::to_string(from.port));
            send(transport, from, resp);
            return;
        }
        case turn_auth::AuthResult::WrongCredentials: {
            auto resp = message::make_error(
                msg.transaction_id, 441, "Wrong Credentials");
            LOGE("[turn] auth failed (wrong credentials)"
                 " from " + from.address + ":" + std::to_string(from.port));
            send(transport, from, resp);
            return;
        }
        case turn_auth::AuthResult::Ok:
            break;
    }

    std::string username;
    auto username_attr = msg.findAttr(message::AttrType::Username);
    if (username_attr)
        username = std::string(username_attr->value.begin(),
                               username_attr->value.end());

    std::string password = hmac_validator_.getPassword(username);
    std::vector<uint8_t> resp;

    switch (msg.method) {
        case message::Method::Allocate: {
            std::string realm_str = long_term_cred_.realm();
            resp = alloc_manager_.handleAllocate(msg, from, username, realm_str);
            if (resp.size() >= 2) {
                message::TurnMessage r;
                message::parse(resp.data(), resp.size(), r);
                if (r.msg_class == message::MessageClass::SuccessResponse) {
                    LOG("[turn] allocate success"
                        " user=" + username +
                        " client=" + from.address + ":" +
                        std::to_string(from.port));
                } else {
                    LOGE("[turn] allocate failed"
                         " user=" + username +
                         " client=" + from.address + ":" +
                         std::to_string(from.port));
                }
            }
            break;
        }
        case message::Method::Refresh:
            resp = alloc_manager_.handleRefresh(msg, from, username);
            break;

        case message::Method::CreatePermission:
            resp = alloc_manager_.handleCreatePermission(msg, from);
            break;

        case message::Method::ChannelBind:
            resp = alloc_manager_.handleChannelBind(msg, from);
            break;

        default:
            resp = message::make_error(msg.transaction_id, 400, "Bad Request");
            break;
    }

    if (!resp.empty())
        send(transport, from, signResponse(resp, username, password));
}

void TurnDispatcher::handleChannelData(const uint8_t*             data,
                                        size_t                     size,
                                        const transport::Endpoint& from,
                                        transport::ITransport&     transport) {
    message::ChannelDataMessage ch;
    auto r = message::parse_channel_data(data, size, ch);
    if (r != message::ChannelDataResult::Ok) return;

    auto alloc = alloc_manager_.findByClient(from);
    if (!alloc) return;

    auto binding_it = alloc->channels.find(ch.channel_number);
    if (binding_it == alloc->channels.end()) return;

    transport::Endpoint peer = binding_it->second.peer;

    if (!alloc->hasPermission(peer.address)) {
        LOGE("[turn] channel data denied: no permission for " + peer.address);
        return;
    }

    auto peer_alloc = alloc_manager_.findByRelay(peer);
    if (peer_alloc) {
        uint16_t peer_channel = 0;
        for (auto& [num, bind] : peer_alloc->channels) {
            if (bind.peer.address == alloc->relayedAddr.address &&
                bind.peer.port   == alloc->relayedAddr.port) {
                peer_channel = num;
                break;
            }
        }

        if (peer_channel != 0) {
            auto fwd = message::make_channel_data(peer_channel, ch.data);
            transport.send(fwd, peer_alloc->clientAddr);
        } else {
            boost::system::error_code ec;
            auto relay_v4 = boost::asio::ip::make_address_v4(
                alloc->relayedAddr.address, ec);
            if (!ec) {
                std::array<uint8_t, 12> tid{};
                auto indication = message::make_data_indication(
                    tid, relay_v4.to_uint(),
                    alloc->relayedAddr.port, ch.data);
                transport.send(indication, peer_alloc->clientAddr);
            }
        }
        return;
    }

    transport.send(ch.data, peer);
}

std::vector<uint8_t> TurnDispatcher::signResponse(
    const std::vector<uint8_t>& resp,
    const std::string&          username,
    const std::string&          password) const
{
    if (resp.size() < 20) return resp;

    uint16_t type = (static_cast<uint16_t>(resp[0]) << 8) | resp[1];
    if (message::decodeClass(type) != message::MessageClass::SuccessResponse)
        return resp;

    auto key_bytes = crypto::long_term_key_md5(
        username, long_term_cred_.realm(), password);
    std::vector<uint8_t> key_vec(key_bytes.begin(), key_bytes.end());

    size_t mi_pos = resp.size();
    uint16_t adjusted_len = static_cast<uint16_t>(mi_pos - 20 + 4 + 20);

    std::vector<uint8_t> buf(resp.begin(), resp.begin() + mi_pos);
    buf[2] = (adjusted_len >> 8) & 0xFF;
    buf[3] =  adjusted_len       & 0xFF;

    std::string buf_str(buf.begin(), buf.end());
    auto mi = crypto::hmac_sha1_bytes(key_vec, buf_str);

    std::vector<uint8_t> result = resp;
    result[2] = (adjusted_len >> 8) & 0xFF;
    result[3] =  adjusted_len       & 0xFF;
    result.push_back(0x00); result.push_back(0x08);
    result.push_back(0x00); result.push_back(0x14);
    for (auto b : mi) result.push_back(b);

    return result;
}