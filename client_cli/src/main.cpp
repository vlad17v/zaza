#include "cli/repl.hpp"
#include "cli/commands.hpp"
#include "session/session.hpp"
#include "signaling/ws_client.hpp"
#include "signaling/message_handler.hpp"
#include "rtc/peer_connection.hpp"
#include "rtc/sdp_handler.hpp"
#include "rtc/ice_handler.hpp"

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

    signaling::WsClient       ws_client("localhost", 8080);
    signaling::MessageHandler handler(session, repl);

    auto ws_send = [&ws_client](const std::string& msg) {
        ws_client.send(msg);
    };

    std::unique_ptr<rtc_client::PeerConnection> pc;
    std::unique_ptr<rtc_client::SdpHandler>     sdp_handler;
    std::unique_ptr<rtc_client::IceHandler>     ice_handler;

    auto initRtc = [&]() {
        pc          = std::make_unique<rtc_client::PeerConnection>(session, repl);
        pc->init();
        sdp_handler = std::make_unique<rtc_client::SdpHandler>(*pc, session, repl, ws_send);
        ice_handler = std::make_unique<rtc_client::IceHandler> (*pc, session, repl, ws_send);

        handler.onOffer ([&](const std::string& sdp) { sdp_handler->handleOffer(sdp);  });
        handler.onAnswer([&](const std::string& sdp) { sdp_handler->handleAnswer(sdp); });
        handler.onIce   ([&](const std::string& c, const std::string& m, int ml) {
            ice_handler->handleRemoteCandidate(c, m, ml);
        });
    };

    auto original_on_message = [&](const std::string& msg) {
        handler.handle(msg);
    };

    ws_client.onMessage([&](const std::string& msg) {
        try {
            auto j = nlohmann::json::parse(msg);
            if (j.value("type", "") == "rtc.config") {
                handler.handle(msg);
                initRtc();

                if (session.call.state == session::AppState::Calling)
                    sdp_handler->startCall();
                return;
            }
        } catch (...) {}

        handler.handle(msg);
    });

    ws_client.onClose([&repl]() {
        repl.print("[ws] disconnected, reconnecting...");
    });

    repl.commands().setWsSend(ws_send);

    repl.setOnLogin([&ws_client, &session]() {
        try {
            ws_client.connect(session.jwt);
        } catch (const std::exception& e) {
            std::cerr << "[ws] connect failed: " << e.what() << "\n";
        }
    });

    repl.run();

    if (pc) pc->close();

    return 0;
}