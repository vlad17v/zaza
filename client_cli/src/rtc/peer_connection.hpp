#pragma once

#include "session/session.hpp"
#include "cli/repl.hpp"

#include <rtc/rtc.hpp>

#include <string>
#include <memory>
#include <functional>
#include <atomic>

namespace rtc_client {

using OfferCallback  = std::function<void(const std::string& sdp)>;
using AnswerCallback = std::function<void(const std::string& sdp)>;
using IceCallback    = std::function<void(const std::string& candidate,
                                          const std::string& mid,
                                          int                mlineindex)>;
using TrackCallback  = std::function<void(std::shared_ptr<rtc::Track>)>;

class PeerConnection {
public:
    PeerConnection(session::Session& session,
                   cli::Repl&        repl);

    ~PeerConnection();

    void init();

    void createOffer();

    void applyOffer(const std::string& sdp);

    void applyAnswer(const std::string& sdp);

    void addRemoteCandidate(const std::string& candidate,
                            const std::string& mid,
                            int                mlineindex);

    void close();

    bool isConnected() const { return connected_.load(); }

    void onOffer (OfferCallback  cb) { on_offer_  = std::move(cb); }
    void onAnswer(AnswerCallback cb) { on_answer_ = std::move(cb); }
    void onIce   (IceCallback    cb) { on_ice_    = std::move(cb); }
    void onTrack (TrackCallback  cb) { on_track_  = std::move(cb); }

    std::shared_ptr<rtc::Track> audioTrack() { return audio_track_; }

private:
    void setupCallbacks();
    void handleStateChange(rtc::PeerConnection::State state);

    session::Session& session_;
    cli::Repl&        repl_;

    std::shared_ptr<rtc::PeerConnection> pc_;
    std::shared_ptr<rtc::Track>          audio_track_;

    std::atomic<bool> connected_{false};
    int               reconnect_attempts_{0};
    static constexpr int kMaxReconnectAttempts = 3;

    OfferCallback  on_offer_;
    AnswerCallback on_answer_;
    IceCallback    on_ice_;
    TrackCallback  on_track_;
};

}