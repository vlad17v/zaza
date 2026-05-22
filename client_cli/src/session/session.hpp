#pragma once

#include "app_state.hpp"
#include <string>

namespace session {

struct TurnConfig {
    std::string turnUrl;
    std::string turnsUrl;
    std::string username;
    std::string credential;
    int64_t     ttl = 3600;
};

struct CallContext {
    std::string callId;
    std::string remoteUser;
    AppState    state = AppState::Idle;
    bool        muted = false;
};

struct RecordContext {
    bool        active   = false;
    std::string filename;
};

struct Session {
    std::string   userId;
    std::string   jwt;
    TurnConfig    turnConfig;
    CallContext   call;
    RecordContext record;

    bool isLoggedIn() const { return !jwt.empty(); }
    bool isInCall()   const { return call.state != AppState::Idle; }
};

}