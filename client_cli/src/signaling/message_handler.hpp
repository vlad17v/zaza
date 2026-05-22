#pragma once

#include "session/session.hpp"
#include "cli/repl.hpp"

#include <string>
#include <functional>
#include <nlohmann/json.hpp>

namespace signaling {

using SdpCallback = std::function<void(const std::string& sdp)>;
using IceCallback = std::function<void(const std::string& candidate,
                                        const std::string& mid,
                                        int                mlineindex)>;

class MessageHandler {
public:
    MessageHandler(session::Session& session,
                   cli::Repl&        repl);

    void handle(const std::string& json);

    void onOffer (SdpCallback cb) { on_offer_  = std::move(cb); }
    void onAnswer(SdpCallback cb) { on_answer_ = std::move(cb); }
    void onIce   (IceCallback cb) { on_ice_    = std::move(cb); }

private:
    void handleCallIncoming (const nlohmann::json& msg);
    void handleCallCreated  (const nlohmann::json& msg);
    void handleCallEnded    (const nlohmann::json& msg);
    void handleCallFailed   (const nlohmann::json& msg);
    void handleRtcConfig    (const nlohmann::json& msg);
    void handleWebrtcOffer  (const nlohmann::json& msg);
    void handleWebrtcAnswer (const nlohmann::json& msg);
    void handleWebrtcIce    (const nlohmann::json& msg);
    void handleError        (const nlohmann::json& msg);

    session::Session& session_;
    cli::Repl&        repl_;

    SdpCallback on_offer_;
    SdpCallback on_answer_;
    IceCallback on_ice_;
};

}