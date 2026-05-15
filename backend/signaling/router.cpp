#include "router.hpp"

#include <nlohmann/json.hpp>
#include <iostream>

namespace signaling {

using json = nlohmann::json;

Router::Router(WsServer&           ws_server,
               calls::CallManager& call_manager,
               RtcConfigGenerator  rtc_config_gen)
    : ws_(ws_server)
    , call_manager_(call_manager)
    , rtc_config_gen_(std::move(rtc_config_gen))
{}

void Router::sendJson(const std::string& userId, const json& msg) {
    ws_.send(userId, msg.dump());
}

void Router::sendError(const std::string& userId,
                        const std::string& reason) {
    sendJson(userId, {{"type", "error"}, {"message", reason}});
}

void Router::handle(const std::string& userId, const std::string& raw) {
    json msg;
    try {
        msg = json::parse(raw);
    } catch (...) {
        sendError(userId, "invalid json");
        return;
    }

    if (!msg.contains("type") || !msg["type"].is_string()) {
        sendError(userId, "missing type");
        return;
    }

    std::string type = msg["type"];

    if      (type == "call.create")   onCallCreate (userId, msg);
    else if (type == "call.accept")   onCallAccept (userId, msg);
    else if (type == "call.reject")   onCallReject (userId, msg);
    else if (type == "call.hangup")   onCallHangup (userId, msg);
    else if (type == "webrtc.offer"  ||
             type == "webrtc.answer" ||
             type == "webrtc.ice")    onWebrtcRelay(userId, msg, type);
    else
        sendError(userId, "unknown type: " + type);
}

void Router::onCallCreate(const std::string& userId, const json& msg) {
    if (!msg.contains("to") || !msg["to"].is_string()) {
        sendError(userId, "missing to");
        return;
    }
    std::string calleeId = msg["to"];

    std::string callId;
    try {
        callId = call_manager_.create(userId, calleeId);
    } catch (const std::exception& e) {
        sendError(userId, e.what());
        return;
    }

    sendJson(userId, {
        {"type",   "call.created"},
        {"callId", callId}
    });

    sendJson(calleeId, {
        {"type",   "call.incoming"},
        {"callId", callId},
        {"from",   userId}
    });
}

void Router::onCallAccept(const std::string& userId, const json& msg) {
    if (!msg.contains("callId") || !msg["callId"].is_string()) {
        sendError(userId, "missing callId");
        return;
    }
    std::string callId = msg["callId"];

    calls::CallSession session;
    try {
        session = call_manager_.accept(callId, userId);
    } catch (const std::exception& e) {
        sendError(userId, e.what());
        return;
    }

    auto callerConfig = rtc_config_gen_(session.callerId);
    auto calleeConfig = rtc_config_gen_(session.calleeId);

    json caller_rtc = json::parse(callerConfig);
    caller_rtc["type"]   = "rtc.config";
    caller_rtc["callId"] = callId;

    json callee_rtc = json::parse(calleeConfig);
    callee_rtc["type"]   = "rtc.config";
    callee_rtc["callId"] = callId;

    sendJson(session.callerId, caller_rtc);
    sendJson(session.calleeId, callee_rtc);
}

void Router::onCallReject(const std::string& userId, const json& msg) {
    if (!msg.contains("callId") || !msg["callId"].is_string()) {
        sendError(userId, "missing callId");
        return;
    }
    std::string callId = msg["callId"];

    calls::CallSession session;
    try {
        session = call_manager_.reject(callId, userId);
    } catch (const std::exception& e) {
        sendError(userId, e.what());
        return;
    }

    sendJson(session.callerId, {
        {"type",   "call.ended"},
        {"callId", callId},
        {"reason", "rejected"}
    });
}

void Router::onCallHangup(const std::string& userId, const json& msg) {
    if (!msg.contains("callId") || !msg["callId"].is_string()) {
        sendError(userId, "missing callId");
        return;
    }
    std::string callId = msg["callId"];

    calls::CallSession session;
    try {
        session = call_manager_.hangup(callId, userId);
    } catch (const std::exception& e) {
        sendError(userId, e.what());
        return;
    }

    std::string otherId = (session.callerId == userId)
                        ? session.calleeId
                        : session.callerId;

    sendJson(otherId, {
        {"type",   "call.ended"},
        {"callId", callId},
        {"reason", "hangup"}
    });
}

void Router::onWebrtcRelay(const std::string& userId,
                            const json&        msg,
                            const std::string& type) {
    if (!msg.contains("callId") || !msg["callId"].is_string()) {
        sendError(userId, "missing callId");
        return;
    }
    std::string callId = msg["callId"];

    auto session = call_manager_.find(callId);
    if (!session) {
        sendError(userId, "call not found");
        return;
    }

    std::string otherId = (session->callerId == userId)
                        ? session->calleeId
                        : session->callerId;

    ws_.send(otherId, msg.dump());
}

}