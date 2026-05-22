#include "cli/repl.hpp"
#include "session/session.hpp"

#include <csignal>
#include <iostream>

static cli::Repl* g_repl    = nullptr;
static session::Session* g_session = nullptr;

static void signal_handler(int) {
    if (g_session && g_session->isInCall()) {
        std::cout << "\n[signal] hangup\n";
        g_session->call = session::CallContext{};
    }
    if (g_repl)
        g_repl->requestQuit();
}

int main() {
    session::Session session;
    cli::Repl repl(session);

    g_repl    = &repl;
    g_session = &session;

    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    repl.run();
    return 0;
}