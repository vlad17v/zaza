#pragma once

namespace session {

enum class AppState {
    Idle,
    Calling,
    Ringing,
    InCall,
};

inline const char* toString(AppState s) {
    switch (s) {
        case AppState::Idle:    return "Idle";
        case AppState::Calling: return "Calling";
        case AppState::Ringing: return "Ringing";
        case AppState::InCall:  return "InCall";
        default:                return "Unknown";
    }
}

}