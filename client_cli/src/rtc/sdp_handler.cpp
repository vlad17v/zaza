#include "sdp_handler.hpp"

#include <nlohmann/json.hpp>

namespace rtc_client {

using json = nlohmann::json;

SdpHandler::SdpHandler(PeerConnection&   pc,
                        session::Session& session,
                        cli::Repl&        repl,
                        WsSendCallback    ws_send)
    : pc_(pc)
    , session_(session)
    , repl_(repl)
    , ws_send_(std::move(ws_send))
{
    pc_.onOffer([this](const std::string& sdp) {
        json msg = {
            {"type",   "webrtc.offer"},
            {"callId", session_.call.callId},
            {"sdp",    sdp}
        };
        if (ws_send_) ws_send_(msg.dump());
        repl_.print("[rtc] offer sent");
    });

    pc_.onAnswer([this](const std::string& sdp) {
        json msg = {
            {"type",   "webrtc.answer"},
            {"callId", session_.call.callId},
            {"sdp",    sdp}
        };
        if (ws_send_) ws_send_(msg.dump());
        repl_.print("[rtc] answer sent");
    });
}

void SdpHandler::startCall() {
    pc_.createOffer();
}

void SdpHandler::handleOffer(const std::string& sdp) {
    pc_.applyOffer(sdp);
}

void SdpHandler::handleAnswer(const std::string& sdp) {
    pc_.applyAnswer(sdp);
    repl_.print("[rtc] answer applied");
}

}