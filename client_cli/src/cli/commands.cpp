#include "commands.hpp"
#include <iostream>

namespace cli {

Commands::Commands(session::Session& session)
    : session_(session)
{}

CommandResult Commands::login(const std::vector<std::string>& args) {
    if (args.size() < 2)
        return {false, "usage: login <userId> <password>"};

    // Заглушка — реальная логика в этапе 2
    std::cout << "[stub] login " << args[0] << "\n";
    session_.userId = args[0];
    session_.jwt    = "stub_jwt";
    return {true, "logged in as " + args[0]};
}

CommandResult Commands::call(const std::vector<std::string>& args) {
    if (args.empty())
        return {false, "usage: call <userId>"};
    if (!session_.isLoggedIn())
        return {false, "not logged in"};
    if (session_.isInCall())
        return {false, "already in call"};

    std::cout << "[stub] call " << args[0] << "\n";
    session_.call.remoteUser = args[0];
    session_.call.state      = session::AppState::Calling;
    session_.call.callId     = "stub_call_id";
    return {true, "calling " + args[0]};
}

CommandResult Commands::accept(const std::vector<std::string>& args) {
    if (session_.call.state != session::AppState::Ringing)
        return {false, "no incoming call"};

    std::cout << "[stub] accept\n";
    session_.call.state = session::AppState::InCall;
    return {true, "call accepted"};
}

CommandResult Commands::reject(const std::vector<std::string>& args) {
    if (session_.call.state != session::AppState::Ringing)
        return {false, "no incoming call"};

    std::cout << "[stub] reject\n";
    session_.call = session::CallContext{};
    return {true, "call rejected"};
}

CommandResult Commands::hangup(const std::vector<std::string>& args) {
    if (!session_.isInCall())
        return {false, "not in call"};

    std::cout << "[stub] hangup\n";
    session_.call = session::CallContext{};
    return {true, "call ended"};
}

CommandResult Commands::mute(const std::vector<std::string>& args) {
    if (session_.call.state != session::AppState::InCall)
        return {false, "not in call"};
    if (session_.call.muted)
        return {false, "already muted"};

    session_.call.muted = true;
    return {true, "muted"};
}

CommandResult Commands::unmute(const std::vector<std::string>& args) {
    if (session_.call.state != session::AppState::InCall)
        return {false, "not in call"};
    if (!session_.call.muted)
        return {false, "not muted"};

    session_.call.muted = false;
    return {true, "unmuted"};
}

CommandResult Commands::status(const std::vector<std::string>& args) {
    std::string msg;
    msg += "user:  " + (session_.userId.empty() ? "(not logged in)" : session_.userId) + "\n";
    msg += "state: " + std::string(session::toString(session_.call.state));
    if (session_.isInCall()) {
        msg += "\ncall:  " + session_.call.callId;
        msg += "\npeer:  " + session_.call.remoteUser;
        msg += "\nmuted: " + std::string(session_.call.muted ? "yes" : "no");
    }
    return {true, msg};
}

CommandResult Commands::quit(const std::vector<std::string>& args) {
    if (session_.isInCall()) {
        session_.call = session::CallContext{};
        std::cout << "[stub] hangup before quit\n";
    }
    quit_ = true;
    return {true, "bye"};
}

CommandResult Commands::record(const std::vector<std::string>& args) {
    if (args.empty())
        return {false, "usage: record <filename.wav>"};
    if (session_.record.active)
        return {false, "already recording, use stop first"};

    std::string filename = args[0];
    if (filename.size() < 4 ||
        filename.substr(filename.size() - 4) != ".wav")
        return {false, "only .wav format supported"};

    std::cout << "[stub] record → " << filename << "\n";
    session_.record.active   = true;
    session_.record.filename = filename;
    return {true, "recording to " + filename};
}

CommandResult Commands::stop(const std::vector<std::string>& args) {
    if (!session_.record.active)
        return {false, "not recording"};

    std::cout << "[stub] stop recording\n";
    auto filename = session_.record.filename;
    session_.record = session::RecordContext{};
    return {true, "saved to " + filename};
}

CommandResult Commands::sendfile(const std::vector<std::string>& args) {
    if (args.empty())
        return {false, "usage: sendfile <filename.wav>"};
    if (!session_.isInCall())
        return {false, "not in call"};

    std::string filename = args[0];
    if (filename.size() < 4 ||
        filename.substr(filename.size() - 4) != ".wav")
        return {false, "only .wav format supported"};

    std::cout << "[stub] sendfile " << filename << "\n";
    return {true, "sending " + filename};
}

}