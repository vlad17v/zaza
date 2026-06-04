#include "repl.hpp"

#include <iostream>
#include <string>
#include <thread>

namespace cli {

Repl::Repl(session::Session& session)
    : session_(session)
    , commands_(session)
{}

void Repl::setOnLogin(LoginCallback cb) {
    on_login_ = std::move(cb);
}

Commands& Repl::commands() {
    return commands_;
}

void Repl::print(const std::string& message) {
    std::lock_guard<std::mutex> lock(output_mutex_);
    std::cout << "\n" << message << "\n";
    prompt();
}

void Repl::prompt() {
    if (session_.isLoggedIn())
        std::cout << "[" << session_.userId << "] > " << std::flush;
    else
        std::cout << "> " << std::flush;
}

void Repl::processLine(const std::string& line) {
    if (line.empty()) return;

    auto cmd = parse(line);
    if (cmd.empty()) return;

    CommandResult result;

    if (cmd.name == "login") {
        result = commands_.login(cmd.args);
        if (result.ok && on_login_) on_login_();
    }
    else if (cmd.name == "call")     result = commands_.call    (cmd.args);
    else if (cmd.name == "accept")   result = commands_.accept  (cmd.args);
    else if (cmd.name == "reject")   result = commands_.reject  (cmd.args);
    else if (cmd.name == "hangup")   result = commands_.hangup  (cmd.args);
    else if (cmd.name == "mute")     result = commands_.mute    (cmd.args);
    else if (cmd.name == "unmute")   result = commands_.unmute  (cmd.args);
    else if (cmd.name == "record")   result = commands_.record  (cmd.args);
    else if (cmd.name == "register") result = commands_.register_(cmd.args);
    else if (cmd.name == "stop")     result = commands_.stop    (cmd.args);
    else if (cmd.name == "sendfile") result = commands_.sendfile(cmd.args);
    else if (cmd.name == "status")   result = commands_.status  (cmd.args);
    else if (cmd.name == "quit" ||
             cmd.name == "exit")     result = commands_.quit    (cmd.args);
    else
        result = {false, "unknown command: " + cmd.name +
                 " (try: login register call accept reject hangup "
                 "mute unmute record stop sendfile status quit)"};

    {
        std::lock_guard<std::mutex> lock(output_mutex_);
        if (!result.message.empty())
            std::cout << result.message << "\n";
    }

    if (commands_.shouldQuit())
        quit_.store(true);
}

void Repl::run() {
    std::thread stdin_thread([this]() {
        std::string line;
        while (!quit_.load()) {
            {
                std::lock_guard<std::mutex> lock(output_mutex_);
                prompt();
            }
            if (!std::getline(std::cin, line)) break;
            processLine(line);
        }
    });

    stdin_thread.join();
}

void Repl::requestQuit() {
    quit_.store(true);
}

}