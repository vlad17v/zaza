#include "commands.hpp"
#include "http/http_client.hpp"
#include "config/config.hpp"

#include <nlohmann/json.hpp>
#include <iostream>

namespace cli {

using json = nlohmann::json;

Commands::Commands(session::Session& session)
    : session_(session)
{}

CommandResult Commands::login(const std::vector<std::string>& args) {
    if (args.size() < 2)
        return {false, "usage: login <userId> <password>"};

    try {
        net::HttpClient client(CFG_DEF("SERVER_HOST", "localhost"),
                       CFG_INT("SERVER_PORT", 8080));

        json body = {{"userId",   args[0]},
                     {"password", args[1]}};

        auto resp = client.post("/api/auth/login", body.dump());

        if (resp.status != 200)
            return {false, "login failed: " + resp.body};

        auto j = json::parse(resp.body);
        session_.userId = j.value("userId", args[0]);
        session_.jwt    = j.value("token",  "");

        if (session_.jwt.empty())
            return {false, "login failed: no token in response"};

        return {true, "logged in as " + session_.userId};

    } catch (const std::exception& e) {
        return {false, std::string("login error: ") + e.what()};
    }
}

CommandResult Commands::call(const std::vector<std::string>& args) {
    if (args.empty())
        return {false, "usage: call <userId>"};
    if (!session_.isLoggedIn())
        return {false, "not logged in"};
    if (args[0] == session_.userId)
        return {false, "cannot call yourself"};
    if (session_.isInCall())
        return {false, "already in call"};

    session_.call.remoteUser = args[0];
    session_.call.state      = session::AppState::Calling;
    session_.call.is_caller = true;

    if (ws_send_) {
        json msg = {{"type", "call.create"},
                    {"to",   args[0]}};
        ws_send_(msg.dump());
    }

    return {true, "calling " + args[0]};
}

CommandResult Commands::accept(const std::vector<std::string>& args) {
    if (session_.call.state != session::AppState::Ringing)
        return {false, "no incoming call"};

    if (ws_send_) {
        json msg = {{"type",   "call.accept"},
                    {"callId", session_.call.callId}};
        ws_send_(msg.dump());
    }

    session_.call.state = session::AppState::InCall;
    return {true, "call accepted"};
}

CommandResult Commands::reject(const std::vector<std::string>& args) {
    if (session_.call.state != session::AppState::Ringing)
        return {false, "no incoming call"};

    if (ws_send_) {
        json msg = {{"type",   "call.reject"},
                    {"callId", session_.call.callId}};
        ws_send_(msg.dump());
    }

    session_.call = session::CallContext{};
    return {true, "call rejected"};
}

CommandResult Commands::hangup(const std::vector<std::string>& args) {
    if (!session_.isInCall())
        return {false, "not in call"};

    if (ws_send_) {
        json msg = {{"type",   "call.hangup"},
                    {"callId", session_.call.callId}};
        ws_send_(msg.dump());
    }

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
    msg += "user:  " + (session_.userId.empty()
                        ? "(not logged in)" : session_.userId) + "\n";
    msg += "state: " + std::string(session::toString(session_.call.state));
    if (session_.isInCall()) {
        msg += "\ncall:  " + session_.call.callId;
        msg += "\npeer:  " + session_.call.remoteUser;
        msg += "\nmuted: " + std::string(session_.call.muted ? "yes" : "no");
    }
    return {true, msg};
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

    session_.record.active   = true;
    session_.record.filename = filename;

    if (capture_cb_) capture_cb_(true, "");
    return {true, "recording to " + filename};
}

CommandResult Commands::stop(const std::vector<std::string>& args) {
    if (!session_.record.active)
        return {false, "not recording"};

    auto filename = session_.record.filename;
    session_.record = session::RecordContext{};

    if (capture_cb_) capture_cb_(false, filename);
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

    if (sendfile_cb_) sendfile_cb_(filename);
    return {true, "sending " + filename};
}

CommandResult Commands::quit(const std::vector<std::string>& args) {
    if (session_.isInCall()) {
        if (ws_send_) {
            json msg = {{"type",   "call.hangup"},
                        {"callId", session_.call.callId}};
            ws_send_(msg.dump());
        }
        session_.call = session::CallContext{};
    }
    quit_ = true;
    return {true, "bye"};
}

void Commands::setWsSend(WsSendCallback cb) {
    ws_send_ = std::move(cb);
}

void Commands::setCaptureCallback(CaptureCallback cb) {
    capture_cb_ = std::move(cb);
}

void Commands::setSendfileCallback(SendfileCallback cb) {
    sendfile_cb_ = std::move(cb);
}

}