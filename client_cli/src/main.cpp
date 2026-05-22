#include "cli/repl.hpp"
#include "cli/commands.hpp"
#include "session/session.hpp"
#include "signaling/ws_client.hpp"
#include "signaling/message_handler.hpp"

#include <csignal>
#include <iostream>
#include <memory>

static cli::Repl*        g_repl    = nullptr;
static session::Session* g_session = nullptr;

static void signal_handler(int) {
    if (g_session && g_session->isInCall())
        g_session->call = session::CallContext{};
    if (g_repl)
        g_repl->requestQuit();
}

int main() {
    session::Session session;
    cli::Repl        repl(session);

    g_repl    = &repl;
    g_session = &session;

    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    signaling::WsClient      ws_client("localhost", 8080);
    signaling::MessageHandler handler(session, repl);

    repl.commands().setWsSend([&ws_client](const std::string& msg) {
        ws_client.send(msg);
    });

    ws_client.onMessage([&handler](const std::string& msg) {
        handler.handle(msg);
    });

    ws_client.onClose([&repl]() {
        repl.print("[ws] disconnected, reconnecting...");
    });

    repl.setOnLogin([&ws_client, &session]() {
        try {
            ws_client.connect(session.jwt);
        } catch (const std::exception& e) {
            std::cerr << "[ws] connect failed: " << e.what() << "\n";
        }
    });

    repl.run();
    return 0;
}