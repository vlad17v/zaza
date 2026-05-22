#include "message_handler.hpp"

#include <nlohmann/json.hpp>
#include <iostream>

namespace signaling {

using json = nlohmann::json;

MessageHandler::MessageHandler(session::Session& session,
                                cli::Repl&        repl)
    : session_(session)
    , repl_(repl)
{}

void MessageHandler::handle(const std::string& raw) {
    json msg;
    try {
        msg = json::parse(raw);
    } catch (...) {
        repl_.print("[error] invalid json from server");
        return;
    }

    if (!msg.contains("type") || !msg["type"].is_string()) {
        repl_.print("[error] missing type in server message");
        return;
    }

    std::string type = msg["type"];

    if      (type == "call.incoming")  handleCallIncoming (msg);
    else if (type == "call.created")   handleCallCreated  (msg);
    else if (type == "call.ended")     handleCallEnded    (msg);
    else if (type == "call.failed")    handleCallFailed   (msg);
    else if (type == "rtc.config")     handleRtcConfig    (msg);
    else if (type == "webrtc.offer")   handleWebrtcOffer  (msg);
    else if (type == "webrtc.answer")  handleWebrtcAnswer (msg);
    else if (type == "webrtc.ice")     handleWebrtcIce    (msg);
    else if (type == "error")          handleError        (msg);
    else
        repl_.print("[warn] unknown message type: " + type);
}

void MessageHandler::handleCallIncoming(const json& msg) {
    std::string callId = msg.value("callId", "");
    std::string from   = msg.value("from",   "");

    session_.call.callId     = callId;
    session_.call.remoteUser = from;
    session_.call.state      = session::AppState::Ringing;

    repl_.print("incoming call from " + from +
                " (callId: " + callId + ")\n"
                "type 'accept' or 'reject'");
}

void MessageHandler::handleCallCreated(const json& msg) {
    std::string callId = msg.value("callId", "");
    session_.call.callId = callId;
    repl_.print("[call] created, callId: " + callId);
}

void MessageHandler::handleCallEnded(const json& msg) {
    std::string reason = msg.value("reason", "unknown");
    session_.call = session::CallContext{};
    repl_.print("[call] ended, reason: " + reason);
}

void MessageHandler::handleCallFailed(const json& msg) {
    std::string reason = msg.value("reason", "unknown");
    session_.call = session::CallContext{};
    repl_.print("[call] failed: " + reason);
}

void MessageHandler::handleRtcConfig(const json& msg) {
    if (!msg.contains("iceServers") || !msg["iceServers"].is_array()) {
        repl_.print("[error] rtc.config missing iceServers");
        return;
    }

    auto& servers = msg["iceServers"];
    if (servers.empty()) {
        repl_.print("[error] rtc.config empty iceServers");
        return;
    }

    for (auto& server : servers) {
        if (!server.contains("username")) continue;

        auto& urls = server["urls"];
        for (auto& url : urls) {
            std::string u = url;
            if (u.rfind("turn:", 0) == 0 && u.find("turns:") == std::string::npos)
                session_.turnConfig.turnUrl  = u;
            else if (u.rfind("turns:", 0) == 0)
                session_.turnConfig.turnsUrl = u;
        }

        session_.turnConfig.username   = server.value("username",   "");
        session_.turnConfig.credential = server.value("credential", "");
    }

    repl_.print("[rtc] config received, turn: " +
                session_.turnConfig.turnUrl);
}

void MessageHandler::handleWebrtcOffer(const json& msg) {
    if (!msg.contains("sdp")) {
        repl_.print("[error] webrtc.offer missing sdp");
        return;
    }
    if (on_offer_)
        on_offer_(msg["sdp"]);
    else
        repl_.print("[warn] webrtc.offer received but no handler (etap 3)");
}

void MessageHandler::handleWebrtcAnswer(const json& msg) {
    if (!msg.contains("sdp")) {
        repl_.print("[error] webrtc.answer missing sdp");
        return;
    }
    if (on_answer_)
        on_answer_(msg["sdp"]);
    else
        repl_.print("[warn] webrtc.answer received but no handler (etap 3)");
}

void MessageHandler::handleWebrtcIce(const json& msg) {
    if (!msg.contains("candidate")) {
        repl_.print("[error] webrtc.ice missing candidate");
        return;
    }
    if (on_ice_)
        on_ice_(msg["candidate"],
                msg.value("mid", ""),
                msg.value("mlineindex", 0));
    else
        repl_.print("[warn] webrtc.ice received but no handler (etap 3)");
}

void MessageHandler::handleError(const json& msg) {
    std::string error = msg.value("message", msg.value("error", "unknown error"));
    repl_.print("[server error] " + error);
}

}