#pragma once

#include "peer_connection.hpp"
#include "rtc_types.hpp"
#include "session/session.hpp"
#include "cli/repl.hpp"

#include <string>
#include <functional>
#include <memory>

namespace rtc_client {

class SdpHandler {
public:
    SdpHandler(PeerConnection&   pc,
               session::Session& session,
               cli::Repl&        repl,
               WsSendCallback    ws_send);

    void startCall();

    void handleOffer(const std::string& sdp);

    void handleAnswer(const std::string& sdp);

private:
    PeerConnection&   pc_;
    session::Session& session_;
    cli::Repl&        repl_;
    WsSendCallback    ws_send_;
};

}