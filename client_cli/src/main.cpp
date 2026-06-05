#include "cli/repl.hpp"
#include "cli/commands.hpp"
#include "session/session.hpp"
#include "signaling/ws_client.hpp"
#include "signaling/message_handler.hpp"
#include "rtc/peer_connection.hpp"
#include "rtc/sdp_handler.hpp"
#include "rtc/ice_handler.hpp"
#include "audio/capture.hpp"
#include "audio/playback.hpp"
#include "audio/wav.hpp"
#include "config/config.hpp"

#include <nlohmann/json.hpp>
#include <openssl/crypto.h>
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

int main(int argc, char* argv[]) {
    std::string env_file = (argc >= 2) ? argv[1] : "../client.env";
    try {
        Config::instance().load(env_file);
    } catch (const std::exception& e) {
        std::cerr << "[client] config: " << e.what() << " — using defaults\n";
    }

    const std::string server_host = CFG_DEF("SERVER_HOST", "localhost");
    const uint16_t    server_port = CFG_INT("SERVER_PORT", 8080);

    session::Session session;
    cli::Repl        repl(session);

    g_repl    = &repl;
    g_session = &session;

    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    signaling::WsClient       ws_client(server_host, server_port);
    signaling::MessageHandler handler(session, repl);

    auto ws_send = [&ws_client](const std::string& msg) {
        ws_client.send(msg);
    };

    std::unique_ptr<rtc_client::PeerConnection> pc;
    std::unique_ptr<rtc_client::SdpHandler>     sdp_handler;
    std::unique_ptr<rtc_client::IceHandler>     ice_handler;
    std::unique_ptr<audio::Capture>             capture;
    std::unique_ptr<audio::Playback>            playback;

    auto stopAudio = [&]() {
        if (capture)  capture->stop();
        if (playback) playback->stop();
    };

    auto initRtc = [&]() {
        if (pc) {
            try { pc->close(); } catch (...) {}
            pc.reset();
        }
        sdp_handler.reset();
        ice_handler.reset();

        pc = std::make_unique<rtc_client::PeerConnection>(session, repl);
        pc->init();

        pc->onFailed([&]() {
            if (!session.call.callId.empty()) {
                nlohmann::json msg = {
                    {"type",   "call.hangup"},
                    {"callId", session.call.callId}
                };
                ws_send(msg.dump());
            }
            stopAudio();
            session.call = session::CallContext{};
        });

        sdp_handler = std::make_unique<rtc_client::SdpHandler>(
            *pc, session, repl, ws_send);
        ice_handler = std::make_unique<rtc_client::IceHandler>(
            *pc, session, repl, ws_send);

        handler.onOffer([&](const std::string& sdp) {
            sdp_handler->handleOffer(sdp);
        });
        handler.onAnswer([&](const std::string& sdp) {
            sdp_handler->handleAnswer(sdp);
        });
        handler.onIce([&](const std::string& c,
                          const std::string& m,
                          int ml) {
            ice_handler->handleRemoteCandidate(c, m, ml);
        });

        capture = std::make_unique<audio::Capture>();
        capture->start([&](const int16_t* data, size_t count) {
            try {
                if (pc && pc->isConnected() && pc->audioTrack()) {
                    auto bytes = reinterpret_cast<const std::byte*>(data);
                    pc->audioTrack()->send(
                        rtc::binary(bytes, bytes + count * sizeof(int16_t)));
                }
            } catch (...) {}
        });

        playback = std::make_unique<audio::Playback>();
        pc->onTrack([&](std::shared_ptr<rtc::Track> track) {
            playback->start();
            track->onMessage([&](rtc::message_variant data) {
                if (auto* bin = std::get_if<rtc::binary>(&data)) {
                    auto* samples = reinterpret_cast<const int16_t*>(
                        bin->data());
                    playback->write(samples,
                                    bin->size() / sizeof(int16_t));
                }
            });
        });
    };

    repl.commands().setCaptureCallback(
        [&](bool start, const std::string& filename) {
            if (!capture) return;
            if (start) {
                capture->startRecording();
            } else {
                auto samples = capture->stopRecording();
                try {
                    audio::writeWav(filename, samples);
                    repl.print("[audio] saved to " + filename);
                } catch (const audio::WavError& e) {
                    repl.print(std::string("[audio] save error: ") + e.what());
                }
            }
        });

    repl.commands().setSendfileCallback([&](const std::string& filename) {
        try {
            audio::WavHeader header;
            auto samples = audio::readWav(filename, header);
            if (playback)
                playback->playFile(samples);
            repl.print("[audio] sending " + filename
                       + " (" + std::to_string(samples.size()) + " samples)");
        } catch (const audio::WavError& e) {
            repl.print(std::string("[audio] file error: ") + e.what());
        }
    });

    repl.commands().setWsSend(ws_send);

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
            if (j.value("type", "") == "call.ended" ||
                j.value("type", "") == "call.failed") {
                handler.handle(msg);
                stopAudio();
                pc.reset();
                sdp_handler.reset();
                ice_handler.reset();
                return;
            }
        } catch (...) {}
        handler.handle(msg);
    });

    ws_client.onClose([&]() {
        repl.print("[ws] disconnected");
        if (session.isInCall()) {
            stopAudio();
            if (pc) {
                try { pc->close(); } catch (...) {}
            }
            pc.reset();
            sdp_handler.reset();
            ice_handler.reset();
            session.call = session::CallContext{};
            repl.print("[call] ended, reason: server disconnected");
        }
    });

    repl.setOnLogin([&ws_client, &session]() {
        try {
            ws_client.connect(session.jwt);
        } catch (const std::exception& e) {
            std::cerr << "[ws] connect failed: " << e.what() << "\n";
        }
    });

    repl.run();

    OPENSSL_cleanup();

    return 0;
}