#pragma once

#include "ws_server.hpp"
#include "calls/call_manager.hpp"
#include "auth/auth_service.hpp"

#include <string>
#include <functional>
#include <nlohmann/json.hpp>

namespace signaling {

using RtcConfigGenerator = std::function<std::string(const std::string& userId)>;

class Router {
public:
    Router(WsServer&          ws_server,
           calls::CallManager& call_manager,
           RtcConfigGenerator  rtc_config_gen);

    void handle(const std::string& userId,
                const std::string& json);

private:
    void onCallCreate (const std::string& userId,
                       const nlohmann::json& msg);
    void onCallAccept (const std::string& userId,
                       const nlohmann::json& msg);
    void onCallReject (const std::string& userId,
                       const nlohmann::json& msg);
    void onCallHangup (const std::string& userId,
                       const nlohmann::json& msg);
    void onWebrtcRelay(const std::string& userId,
                       const nlohmann::json& msg,
                       const std::string& type);

    void sendJson(const std::string& userId,
                  const nlohmann::json& msg);
    void sendError(const std::string& userId,
                   const std::string& reason);

    void checkExpired();

    WsServer&           ws_;
    calls::CallManager& call_manager_;
    RtcConfigGenerator  rtc_config_gen_;
};

}