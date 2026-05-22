#include "cli/parser.hpp"
#include "cli/commands.hpp"
#include "session/session.hpp"
#include "signaling/message_handler.hpp"
#include "audio/wav.hpp"
#include "audio/jitter_buffer.hpp"

#include <iostream>
#include <cassert>
#include <fstream>

int main() {
    std::cout << "=== Parser ===\n";

    {
        auto cmd = cli::parse("login alice password123");
        assert(cmd.name == "login");
        assert(cmd.args.size() == 2);
        assert(cmd.args[0] == "alice");
        assert(cmd.args[1] == "password123");
        std::cout << "[parser] basic command OK\n";
    }

    {
        auto cmd = cli::parse("call bob");
        assert(cmd.name == "call");
        assert(cmd.args.size() == 1);
        assert(cmd.args[0] == "bob");
        std::cout << "[parser] command with one arg OK\n";
    }

    {
        auto cmd = cli::parse("status");
        assert(cmd.name == "status");
        assert(cmd.args.empty());
        std::cout << "[parser] command no args OK\n";
    }

    {
        auto cmd = cli::parse("");
        assert(cmd.empty());
        std::cout << "[parser] empty line OK\n";
    }

    {
        auto cmd = cli::parse("   ");
        assert(cmd.empty());
        std::cout << "[parser] whitespace only OK\n";
    }

    {
        auto cmd = cli::parse("  login   alice   pass  ");
        assert(cmd.name == "login");
        assert(cmd.args.size() == 2);
        assert(cmd.args[0] == "alice");
        assert(cmd.args[1] == "pass");
        std::cout << "[parser] extra whitespace OK\n";
    }

    std::cout << "\n=== Session structures ===\n";

    {
        session::Session s;
        assert(!s.isLoggedIn());
        assert(!s.isInCall());
        assert(s.call.state == session::AppState::Idle);
        assert(!s.call.muted);
        std::cout << "[session] default state OK\n";
    }

    {
        session::Session s;
        s.userId = "alice";
        s.jwt    = "token123";
        assert(s.isLoggedIn());
        assert(!s.isInCall());
        std::cout << "[session] logged in OK\n";
    }

    {
        session::Session s;
        s.call.state = session::AppState::InCall;
        assert(s.isInCall());
        s.call.state = session::AppState::Calling;
        assert(s.isInCall());
        s.call.state = session::AppState::Ringing;
        assert(s.isInCall());
        s.call.state = session::AppState::Idle;
        assert(!s.isInCall());
        std::cout << "[session] isInCall states OK\n";
    }

    {
        assert(std::string(session::toString(session::AppState::Idle))    == "Idle");
        assert(std::string(session::toString(session::AppState::Calling)) == "Calling");
        assert(std::string(session::toString(session::AppState::Ringing)) == "Ringing");
        assert(std::string(session::toString(session::AppState::InCall))  == "InCall");
        std::cout << "[session] toString OK\n";
    }

    {
        session::TurnConfig tc;
        tc.turnUrl    = "turn:host:3478";
        tc.turnsUrl   = "turns:host:5349";
        tc.username   = "1234:user1";
        tc.credential = "abc123";
        tc.ttl        = 3600;
        assert(tc.turnUrl  == "turn:host:3478");
        assert(tc.turnsUrl == "turns:host:5349");
        assert(tc.ttl      == 3600);
        std::cout << "[session] TurnConfig OK\n";
    }

    std::cout << "\n=== Commands ===\n";

    {
        session::Session s;
        cli::Commands cmd(s);
        auto r = cmd.login({});
        assert(!r.ok);
        std::cout << "[commands] login no args OK\n";
    }

    {
        session::Session s;
        s.userId = "alice";
        s.jwt    = "stub_jwt";
        assert(s.isLoggedIn());
        assert(s.userId == "alice");
        std::cout << "[commands] login state OK\n";
    }

    {
        session::Session s;
        cli::Commands cmd(s);
        auto r = cmd.call({"bob"});
        assert(!r.ok);
        std::cout << "[commands] call without login OK\n";
    }

    {
        session::Session s;
        s.userId = "alice";
        s.jwt    = "stub_jwt";
        cli::Commands cmd(s);
        auto r = cmd.call({});
        assert(!r.ok);
        std::cout << "[commands] call no args OK\n";
    }

    {
        session::Session s;
        s.userId = "alice";
        s.jwt    = "stub_jwt";
        cli::Commands cmd(s);
        auto r = cmd.call({"bob"});
        assert(r.ok);
        assert(s.call.state      == session::AppState::Calling);
        assert(s.call.remoteUser == "bob");
        std::cout << "[commands] call OK\n";
    }

    {
        session::Session s;
        s.userId         = "alice";
        s.jwt            = "stub_jwt";
        s.call.state     = session::AppState::Calling;
        s.call.remoteUser = "bob";
        cli::Commands cmd(s);
        auto r = cmd.call({"carol"});
        assert(!r.ok);
        std::cout << "[commands] call while in call OK\n";
    }

    {
        session::Session s;
        cli::Commands cmd(s);

        auto r = cmd.accept({});
        assert(!r.ok);
        std::cout << "[commands] accept no ringing OK\n";
    }

    {
        session::Session s;
        s.call.state = session::AppState::Ringing;
        cli::Commands cmd(s);

        auto r = cmd.accept({});
        assert(r.ok);
        assert(s.call.state == session::AppState::InCall);
        std::cout << "[commands] accept OK\n";
    }

    {
        session::Session s;
        s.call.state = session::AppState::Ringing;
        cli::Commands cmd(s);

        auto r = cmd.reject({});
        assert(r.ok);
        assert(s.call.state == session::AppState::Idle);
        std::cout << "[commands] reject OK\n";
    }

    {
        session::Session s;
        cli::Commands cmd(s);

        auto r = cmd.reject({});
        assert(!r.ok);
        std::cout << "[commands] reject no ringing OK\n";
    }

    {
        session::Session s;
        s.call.state = session::AppState::InCall;
        cli::Commands cmd(s);

        auto r = cmd.hangup({});
        assert(r.ok);
        assert(s.call.state == session::AppState::Idle);
        std::cout << "[commands] hangup OK\n";
    }

    {
        session::Session s;
        cli::Commands cmd(s);

        auto r = cmd.hangup({});
        assert(!r.ok);
        std::cout << "[commands] hangup not in call OK\n";
    }

    {
        session::Session s;
        s.call.state = session::AppState::InCall;
        cli::Commands cmd(s);

        auto r = cmd.mute({});
        assert(r.ok);
        assert(s.call.muted);

        r = cmd.mute({});
        assert(!r.ok);
        std::cout << "[commands] mute OK\n";
    }

    {
        session::Session s;
        s.call.state = session::AppState::InCall;
        s.call.muted = true;
        cli::Commands cmd(s);

        auto r = cmd.unmute({});
        assert(r.ok);
        assert(!s.call.muted);

        r = cmd.unmute({});
        assert(!r.ok);
        std::cout << "[commands] unmute OK\n";
    }

    {
        session::Session s;
        cli::Commands cmd(s);

        auto r = cmd.mute({});
        assert(!r.ok);
        std::cout << "[commands] mute not in call OK\n";
    }

    {
        session::Session s;
        s.userId    = "alice";
        s.jwt       = "token";
        cli::Commands cmd(s);

        auto r = cmd.status({});
        assert(r.ok);
        assert(r.message.find("alice") != std::string::npos);
        assert(r.message.find("Idle")  != std::string::npos);
        std::cout << "[commands] status logged in OK\n";
    }

    {
        session::Session s;
        s.userId         = "alice";
        s.jwt            = "token";
        s.call.state     = session::AppState::InCall;
        s.call.callId    = "call123";
        s.call.remoteUser = "bob";
        s.call.muted     = true;
        cli::Commands cmd(s);

        auto r = cmd.status({});
        assert(r.ok);
        assert(r.message.find("InCall") != std::string::npos);
        assert(r.message.find("bob")    != std::string::npos);
        assert(r.message.find("yes")    != std::string::npos);
        std::cout << "[commands] status in call OK\n";
    }

    {
        session::Session s;
        cli::Commands cmd(s);

        assert(!cmd.shouldQuit());
        auto r = cmd.quit({});
        assert(r.ok);
        assert(cmd.shouldQuit());
        std::cout << "[commands] quit OK\n";
    }

    {
        session::Session s;
        s.call.state = session::AppState::InCall;
        cli::Commands cmd(s);

        auto r = cmd.quit({});
        assert(r.ok);
        assert(cmd.shouldQuit());
        assert(s.call.state == session::AppState::Idle);
        std::cout << "[commands] quit while in call OK\n";
    }

    std::cout << "\n=== Record / sendfile commands ===\n";

    {
        session::Session s;
        cli::Commands cmd(s);
        auto r = cmd.record({});
        assert(!r.ok);
        std::cout << "[commands] record no args OK\n";
    }

    {
        session::Session s;
        cli::Commands cmd(s);
        auto r = cmd.record({"file.mp3"});
        assert(!r.ok);
        std::cout << "[commands] record wrong format OK\n";
    }

    {
        session::Session s;
        cli::Commands cmd(s);
        auto r = cmd.record({"file.wav"});
        assert(r.ok);
        assert(s.record.active);
        assert(s.record.filename == "file.wav");
        std::cout << "[commands] record OK\n";
    }

    {
        session::Session s;
        cli::Commands cmd(s);
        cmd.record({"file.wav"});
        auto r = cmd.record({"other.wav"});
        assert(!r.ok);
        std::cout << "[commands] record while recording OK\n";
    }

    {
        session::Session s;
        cli::Commands cmd(s);
        auto r = cmd.stop({});
        assert(!r.ok);
        std::cout << "[commands] stop not recording OK\n";
    }

    {
        session::Session s;
        cli::Commands cmd(s);
        cmd.record({"file.wav"});
        auto r = cmd.stop({});
        assert(r.ok);
        assert(!s.record.active);
        assert(r.message.find("file.wav") != std::string::npos);
        std::cout << "[commands] stop OK\n";
    }

    {
        session::Session s;
        cli::Commands cmd(s);
        auto r = cmd.sendfile({"file.wav"});
        assert(!r.ok);
        std::cout << "[commands] sendfile not in call OK\n";
    }

    {
        session::Session s;
        s.call.state = session::AppState::InCall;
        cli::Commands cmd(s);
        auto r = cmd.sendfile({});
        assert(!r.ok);
        std::cout << "[commands] sendfile no args OK\n";
    }

    {
        session::Session s;
        s.call.state = session::AppState::InCall;
        cli::Commands cmd(s);
        auto r = cmd.sendfile({"audio.mp3"});
        assert(!r.ok);
        std::cout << "[commands] sendfile wrong format OK\n";
    }

    {
        session::Session s;
        s.call.state = session::AppState::InCall;
        cli::Commands cmd(s);
        auto r = cmd.sendfile({"audio.wav"});
        assert(r.ok);
        std::cout << "[commands] sendfile OK\n";
    }

    std::cout << "\n=== MessageHandler ===\n";

    {
        session::Session s;
        cli::Repl repl(s);
        signaling::MessageHandler h(s, repl);

        h.handle(R"({"type":"call.incoming","callId":"abc","from":"bob"})");
        assert(s.call.state      == session::AppState::Ringing);
        assert(s.call.callId     == "abc");
        assert(s.call.remoteUser == "bob");
        std::cout << "[msg_handler] call.incoming OK\n";
    }

    {
        session::Session s;
        cli::Repl repl(s);
        signaling::MessageHandler h(s, repl);

        h.handle(R"({"type":"call.created","callId":"xyz"})");
        assert(s.call.callId == "xyz");
        std::cout << "[msg_handler] call.created OK\n";
    }

    {
        session::Session s;
        s.call.state     = session::AppState::InCall;
        s.call.callId    = "abc";
        s.call.remoteUser = "bob";
        cli::Repl repl(s);
        signaling::MessageHandler h(s, repl);

        h.handle(R"({"type":"call.ended","reason":"hangup"})");
        assert(s.call.state == session::AppState::Idle);
        assert(s.call.callId.empty());
        std::cout << "[msg_handler] call.ended OK\n";
    }

    {
        session::Session s;
        s.call.state = session::AppState::Calling;
        cli::Repl repl(s);
        signaling::MessageHandler h(s, repl);

        h.handle(R"({"type":"call.failed","reason":"timeout"})");
        assert(s.call.state == session::AppState::Idle);
        std::cout << "[msg_handler] call.failed OK\n";
    }

    {
        session::Session s;
        cli::Repl repl(s);
        signaling::MessageHandler h(s, repl);

        h.handle(R"({
            "type": "rtc.config",
            "callId": "abc",
            "iceServers": [{
                "urls": ["turn:host:3478", "turns:host:5349"],
                "username": "123:user1",
                "credential": "abc123"
            }]
        })");

        assert(s.turnConfig.turnUrl    == "turn:host:3478");
        assert(s.turnConfig.turnsUrl   == "turns:host:5349");
        assert(s.turnConfig.username   == "123:user1");
        assert(s.turnConfig.credential == "abc123");
        std::cout << "[msg_handler] rtc.config OK\n";
    }

    {
        session::Session s;
        cli::Repl repl(s);
        signaling::MessageHandler h(s, repl);

        std::string got_sdp;
        h.onOffer([&got_sdp](const std::string& sdp) {
            got_sdp = sdp;
        });

        h.handle(R"({"type":"webrtc.offer","callId":"abc","sdp":"v=0..."})");
        assert(got_sdp == "v=0...");
        std::cout << "[msg_handler] webrtc.offer OK\n";
    }

    {
        session::Session s;
        cli::Repl repl(s);
        signaling::MessageHandler h(s, repl);

        std::string got_sdp;
        h.onAnswer([&got_sdp](const std::string& sdp) {
            got_sdp = sdp;
        });

        h.handle(R"({"type":"webrtc.answer","callId":"abc","sdp":"v=0 answer"})");
        assert(got_sdp == "v=0 answer");
        std::cout << "[msg_handler] webrtc.answer OK\n";
    }

    {
        session::Session s;
        cli::Repl repl(s);
        signaling::MessageHandler h(s, repl);

        std::string got_candidate;
        std::string got_mid;
        int         got_mline = -1;
        h.onIce([&](const std::string& c, const std::string& m, int ml) {
            got_candidate = c;
            got_mid       = m;
            got_mline     = ml;
        });

        h.handle(R"({
            "type": "webrtc.ice",
            "callId": "abc",
            "candidate": "candidate:1234",
            "mid": "audio",
            "mlineindex": 0
        })");

        assert(got_candidate == "candidate:1234");
        assert(got_mid       == "audio");
        assert(got_mline     == 0);
        std::cout << "[msg_handler] webrtc.ice OK\n";
    }

    {
        session::Session s;
        cli::Repl repl(s);
        signaling::MessageHandler h(s, repl);

        h.handle("not json at all");
        h.handle(R"({"no_type": "here"})");
        h.handle(R"({"type": "unknown.event"})");
        std::cout << "[msg_handler] invalid messages no crash OK\n";
    }

    std::cout << "\n=== WebRTC JSON formats ===\n";

    {
        session::Session s;
        s.call.callId = "abc123";

        nlohmann::json j = {
            {"type",   "webrtc.offer"},
            {"callId", s.call.callId},
            {"sdp",    "v=0..."}
        };
        assert(j["type"]   == "webrtc.offer");
        assert(j["callId"] == "abc123");
        assert(j["sdp"]    == "v=0...");
        auto parsed = nlohmann::json::parse(j.dump());
        assert(parsed["type"] == "webrtc.offer");
        std::cout << "[webrtc] offer JSON format OK\n";
    }

    {
        session::Session s;
        s.call.callId = "abc123";

        nlohmann::json j = {
            {"type",   "webrtc.answer"},
            {"callId", s.call.callId},
            {"sdp",    "v=0 answer"}
        };
        assert(j["type"]   == "webrtc.answer");
        assert(j["callId"] == "abc123");
        assert(j["sdp"]    == "v=0 answer");
        auto parsed = nlohmann::json::parse(j.dump());
        assert(parsed["type"] == "webrtc.answer");
        std::cout << "[webrtc] answer JSON format OK\n";
    }

    {
        session::Session s;
        s.call.callId = "abc123";

        nlohmann::json j = {
            {"type",       "webrtc.ice"},
            {"callId",     s.call.callId},
            {"candidate",  "candidate:1234 1 UDP 2122252543 192.168.1.2 54321 typ host"},
            {"mid",        "audio"},
            {"mlineindex", 0}
        };
        assert(j["type"]       == "webrtc.ice");
        assert(j["callId"]     == "abc123");
        assert(j["mid"]        == "audio");
        assert(j["mlineindex"] == 0);
        assert(!j["candidate"].get<std::string>().empty());
        auto parsed = nlohmann::json::parse(j.dump());
        assert(parsed["type"] == "webrtc.ice");
        std::cout << "[webrtc] ICE JSON format OK\n";
    }

    {
        nlohmann::json j = {
            {"type",   "webrtc.ice"},
            {"callId", "abc123"},
            {"candidate",  ""},
            {"mid",        ""},
            {"mlineindex", 0}
        };
        assert(j["candidate"].get<std::string>().empty());
        assert(j["mlineindex"] == 0);
        std::cout << "[webrtc] ICE empty candidate OK\n";
    }

    {
        nlohmann::json rtc = {
            {"type",   "rtc.config"},
            {"callId", "abc123"},
            {"iceServers", nlohmann::json::array({
                {
                    {"urls", {"turn:host:3478", "turns:host:5349"}},
                    {"username",   "1715510400:user1"},
                    {"credential", "abc123=="}
                }
            })}
        };
        assert(rtc["type"]   == "rtc.config");
        assert(rtc["callId"] == "abc123");
        assert(rtc["iceServers"].is_array());
        assert(rtc["iceServers"].size() == 1);
        auto& server = rtc["iceServers"][0];
        assert(server["urls"].size() == 2);
        assert(server["urls"][0] == "turn:host:3478");
        assert(server["urls"][1] == "turns:host:5349");
        assert(server["username"].get<std::string>().find(":") != std::string::npos);
        assert(!server["credential"].get<std::string>().empty());
        std::cout << "[webrtc] rtc.config JSON format OK\n";
    }

    std::cout << "\n=== Protocol JSON files ===\n";

    {
        std::vector<std::string> files = {
            "../protocol/signaling.json",
            "../protocol/rtc_config.json",
            "../protocol/turn_credentials.json",
            "../protocol/error_codes.json",
            "../protocol/auth.json"
        };

        for (auto& path : files) {
            std::ifstream f(path);
            if (!f.is_open()) {
                std::cout << "[protocol] SKIP (not found): " << path << "\n";
                continue;
            }
            try {
                auto j = nlohmann::json::parse(f);
                assert(j.is_object());
                std::cout << "[protocol] valid JSON: " << path << " OK\n";
            } catch (...) {
                assert(false && "invalid JSON in protocol file");
            }
        }
    }

    std::cout << "\n=== WAV ===\n";

    {
        std::vector<int16_t> samples(48000, 0);
        for (size_t i = 0; i < samples.size(); ++i)
            samples[i] = static_cast<int16_t>(i % 32767);

        audio::writeWav("/tmp/test.wav", samples);

        audio::WavHeader header;
        auto read = audio::readWav("/tmp/test.wav", header);

        assert(read.size()          == samples.size());
        assert(header.sampleRate    == 48000);
        assert(header.numChannels   == 1);
        assert(header.bitsPerSample == 16);
        assert(read == samples);
        std::cout << "[wav] write/read round-trip OK\n";
    }

    {
        std::vector<int16_t> empty;
        audio::writeWav("/tmp/test_empty.wav", empty);
        audio::WavHeader header;
        auto read = audio::readWav("/tmp/test_empty.wav", header);
        assert(read.empty());
        assert(header.numSamples == 0);
        std::cout << "[wav] empty file OK\n";
    }

    {
        try {
            audio::WavHeader header;
            audio::readWav("/tmp/nonexistent.wav", header);
            assert(false);
        } catch (const audio::WavError&) {}
        std::cout << "[wav] nonexistent file OK\n";
    }

    {
        std::ofstream bad("/tmp/bad.wav");
        bad << "not a wav file";
        bad.close();
        try {
            audio::WavHeader header;
            audio::readWav("/tmp/bad.wav", header);
            assert(false);
        } catch (const audio::WavError&) {}
        std::cout << "[wav] bad file format OK\n";
    }

    std::cout << "\n=== JitterBuffer ===\n";

    {
        audio::JitterBuffer jb(50, 48000);
        assert(jb.available() == 0);

        std::vector<int16_t> in = {1, 2, 3, 4, 5};
        jb.write(in.data(), in.size());
        assert(jb.available() == 5);

        std::vector<int16_t> out(5, 0);
        jb.read(out.data(), 5);
        assert(out == in);
        assert(jb.available() == 0);
        std::cout << "[jitter] write/read OK\n";
    }

    {
        audio::JitterBuffer jb(50, 48000);
        std::vector<int16_t> out(10, 99);
        jb.read(out.data(), 10);
        for (auto s : out)
            assert(s == 0);
        std::cout << "[jitter] underrun silence OK\n";
    }

    {
        audio::JitterBuffer jb(50, 48000);
        std::vector<int16_t> in = {1, 2, 3};
        jb.write(in.data(), in.size());

        std::vector<int16_t> out(5, 99);
        jb.read(out.data(), 5);
        assert(out[0] == 1);
        assert(out[1] == 2);
        assert(out[2] == 3);
        assert(out[3] == 0);
        assert(out[4] == 0);
        std::cout << "[jitter] partial read with silence OK\n";
    }

    {
        audio::JitterBuffer jb(50, 48000);
        std::vector<int16_t> in = {10, 20, 30};
        jb.write(in.data(), in.size());
        jb.clear();
        assert(jb.available() == 0);
        std::vector<int16_t> out(3, 99);
        jb.read(out.data(), 3);
        for (auto s : out) assert(s == 0);
        std::cout << "[jitter] clear OK\n";
    }

    {
        audio::JitterBuffer jb(1, 48000);
        size_t cap = 48000 * 1 / 1000;

        std::vector<int16_t> big(cap + 100, 42);
        jb.write(big.data(), big.size());
        assert(jb.available() == cap);
        std::cout << "[jitter] overflow protection OK\n";
    }

    std::cout << "\nAll client tests passed\n";
    return 0;
}