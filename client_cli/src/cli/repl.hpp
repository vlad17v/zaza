#pragma once

#include "commands.hpp"
#include "parser.hpp"
#include "session/session.hpp"

#include <string>
#include <mutex>
#include <atomic>

namespace cli {

class Repl {
public:
    explicit Repl(session::Session& session);

    void run();

    void print(const std::string& message);

    bool shouldQuit() const { return quit_.load(); }

    void requestQuit();

private:
    void processLine(const std::string& line);
    void prompt();

    session::Session& session_;
    Commands          commands_;
    std::mutex        output_mutex_;
    std::atomic<bool> quit_{false};
};

}