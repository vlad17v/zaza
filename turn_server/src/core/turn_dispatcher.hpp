#pragma once

#include "transport/transport.hpp"
#include "message/parser.hpp"
#include "message/builder.hpp"
#include "auth/long_term_cred.hpp"
#include "auth/nonce_manager.hpp"
#include "auth/hmac_validator.hpp"
#include "allocation/allocation_manager.hpp"

#include <boost/asio.hpp>
#include <memory>
#include <string>

namespace asio = boost::asio;

class TurnDispatcher {
public:
    TurnDispatcher(asio::io_context&  ioc,
                   const std::string& realm,
                   const std::string& shared_secret,
                   const std::string& relay_address,
                   uint16_t           relay_port_min,
                   uint16_t           relay_port_max);

    void onPacket(const uint8_t*             data,
                  size_t                     size,
                  const transport::Endpoint& from,
                  transport::ITransport&     transport);

private:
    void handleStun(const message::TurnMessage& msg,
                    const uint8_t*              raw,
                    size_t                      raw_len,
                    const transport::Endpoint&  from,
                    transport::ITransport&      transport);

    void handleChannelData(const uint8_t*             data,
                           size_t                     size,
                           const transport::Endpoint& from,
                           transport::ITransport&     transport);

    void send(transport::ITransport&      transport,
              const transport::Endpoint&  to,
              const std::vector<uint8_t>& data);

    turn_auth::NonceManager     nonce_manager_;
    turn_auth::HmacValidator    hmac_validator_;
    turn_auth::LongTermCred     long_term_cred_;
    allocation::AllocationManager alloc_manager_;
};