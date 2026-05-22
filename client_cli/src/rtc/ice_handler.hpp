#pragma once

#include "peer_connection.hpp"
#include "rtc_types.hpp"
#include "session/session.hpp"
#include "cli/repl.hpp"

#include <string>
#include <functional>

namespace rtc_client {

class IceHandler {
public:
    IceHandler(PeerConnection&   pc,
               session::Session& session,
               cli::Repl&        repl,
               WsSendCallback    ws_send);

    void handleRemoteCandidate(const std::string& candidate,
                               const std::string& mid,
                               int                mlineindex);

private:
    PeerConnection&   pc_;
    session::Session& session_;
    cli::Repl&        repl_;
    WsSendCallback    ws_send_;
};

}