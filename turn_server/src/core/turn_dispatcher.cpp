#include "turn_dispatcher.hpp"

#include <iostream>

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
        [this](const std::vector<uint8_t>&,
               const transport::Endpoint&) {
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
            std::cerr << "[turn] parse failed: "
                      << static_cast<int>(result) << "\n";
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
    if (msg.msg_class == message::MessageClass::Indication) {
        if (msg.method == message::Method::Send) {
            alloc_manager_.handleSend(msg, from);
        }
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
            send(transport, from, resp);
            return;
        }
        case turn_auth::AuthResult::WrongCredentials: {
            auto resp = message::make_error(
                msg.transaction_id, 441, "Wrong Credentials");
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

    std::vector<uint8_t> resp;

    switch (msg.method) {
        case message::Method::Allocate: {
            std::string realm_str = long_term_cred_.realm();
            resp = alloc_manager_.handleAllocate(msg, from,
                                                  username, realm_str);

            auto alloc = alloc_manager_.findByClient(from);
            if (alloc) {
                alloc_manager_.setClientSendCallback(
                    [&transport, from_ep = from]
                    (const std::vector<uint8_t>& data,
                     const transport::Endpoint&  to) {
                        transport.send(data, to);
                    });
                alloc_manager_.setPeerSendCallback(
                    [&transport](const std::vector<uint8_t>& data,
                                 const transport::Endpoint&  to) {
                        transport.send(data, to);
                    });
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
        send(transport, from, resp);
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

    auto binding = alloc->findChannelByNumber(ch.channel_number);
    if (!binding) return;

    if (!alloc->hasPermission(binding->peer.address)) return;

    alloc_manager_.setPeerSendCallback(
        [&transport](const std::vector<uint8_t>& d,
                     const transport::Endpoint&  to) {
            transport.send(d, to);
        });

    alloc_manager_.handlePeerData(
        ch.data.data(), ch.data.size(),
        binding->peer,
        alloc->relayedAddr);
}