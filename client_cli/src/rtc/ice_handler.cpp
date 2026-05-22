#include "ice_handler.hpp"

#include <nlohmann/json.hpp>

namespace rtc_client {

using json = nlohmann::json;

IceHandler::IceHandler(PeerConnection&   pc,
                        session::Session& session,
                        cli::Repl&        repl,
                        WsSendCallback    ws_send)
    : pc_(pc)
    , session_(session)
    , repl_(repl)
    , ws_send_(std::move(ws_send))
{
    pc_.onIce([this](const std::string& candidate,
                     const std::string& mid,
                     int                mlineindex) {
        json msg = {
            {"type",       "webrtc.ice"},
            {"callId",     session_.call.callId},
            {"candidate",  candidate},
            {"mid",        mid},
            {"mlineindex", mlineindex}
        };
        if (ws_send_) ws_send_(msg.dump());
    });
}

void IceHandler::handleRemoteCandidate(const std::string& candidate,
                                        const std::string& mid,
                                        int                mlineindex) {
    pc_.addRemoteCandidate(candidate, mid, mlineindex);
}

}