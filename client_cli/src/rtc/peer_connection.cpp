#include "peer_connection.hpp"

#include <iostream>

namespace rtc_client {

PeerConnection::PeerConnection(session::Session& session,
                                cli::Repl&        repl)
    : session_(session)
    , repl_(repl)
{}

PeerConnection::~PeerConnection() {
    if (pc_ && pc_->state() != rtc::PeerConnection::State::Closed)
        close();
}

void PeerConnection::init() {
    rtc::Configuration config;

    rtc::IceServer turn_server(session_.turnConfig.turnUrl);
    turn_server.username   = session_.turnConfig.username;
    turn_server.password   = session_.turnConfig.credential;
    config.iceServers.push_back(turn_server);

    rtc::IceServer turns_server(session_.turnConfig.turnsUrl);
    turns_server.username   = session_.turnConfig.username;
    turns_server.password   = session_.turnConfig.credential;
    config.iceServers.push_back(turns_server);

    config.iceTransportPolicy = rtc::TransportPolicy::Relay;

    pc_ = std::make_shared<rtc::PeerConnection>(config);
    setupCallbacks();
}

void PeerConnection::setupCallbacks() {
    pc_->onLocalDescription([this](rtc::Description desc) {
        std::string sdp = std::string(desc);
        if (desc.type() == rtc::Description::Type::Offer) {
            if (on_offer_) on_offer_(sdp);
        } else if (desc.type() == rtc::Description::Type::Answer) {
            if (on_answer_) on_answer_(sdp);
        }
    });

    pc_->onLocalCandidate([this](rtc::Candidate candidate) {
        if (on_ice_)
            on_ice_(std::string(candidate),
                    candidate.mid(),
                    0);
    });

    pc_->onStateChange([this](rtc::PeerConnection::State state) {
        handleStateChange(state);
    });

    pc_->onTrack([this](std::shared_ptr<rtc::Track> track) {
        if (on_track_) on_track_(track);
    });
}

void PeerConnection::handleStateChange(rtc::PeerConnection::State state) {
    switch (state) {
        case rtc::PeerConnection::State::Connecting:
            repl_.print("[rtc] connecting...");
            break;

        case rtc::PeerConnection::State::Connected:
            connected_.store(true);
            reconnect_attempts_ = 0;
            session_.call.state = session::AppState::InCall;
            repl_.print("[rtc] connected");
            break;

        case rtc::PeerConnection::State::Failed:
            connected_.store(false);
            repl_.print("[rtc] failed — use 'hangup' and call again");
            if (on_failed_) on_failed_();
            break;

        case rtc::PeerConnection::State::Disconnected:
            connected_.store(false);
            session_.call.state = session::AppState::Idle;
            session_.call = session::CallContext{};
            repl_.print("[rtc] disconnected");
            break;

        case rtc::PeerConnection::State::Closed:
            connected_.store(false);
            break;

        default:
            break;
    }
}

void PeerConnection::createOffer() {
    if (!pc_) return;

    auto audio = rtc::Description::Audio("audio");
    audio.addOpusCodec(111);
    audio.setDirection(rtc::Description::Direction::SendRecv);

    audio_track_ = pc_->addTrack(audio);
    pc_->setLocalDescription(rtc::Description::Type::Offer);
}

void PeerConnection::applyOffer(const std::string& sdp) {
    if (!pc_) return;

    rtc::Description offer(sdp, rtc::Description::Type::Offer);
    pc_->setRemoteDescription(offer);
}

void PeerConnection::applyAnswer(const std::string& sdp) {
    if (!pc_) return;

    rtc::Description answer(sdp, rtc::Description::Type::Answer);
    pc_->setRemoteDescription(answer);
}

void PeerConnection::addRemoteCandidate(const std::string& candidate,
                                         const std::string& mid,
                                         int                mlineindex) {
    if (!pc_) return;
    rtc::Candidate c(candidate, mid);
    pc_->addRemoteCandidate(c);
    (void)mlineindex;
}

void PeerConnection::close() {
    if (pc_) {
        try {
            if (pc_->state() != rtc::PeerConnection::State::Closed)
                pc_->close();
        } catch (...) {}
        pc_.reset();
    }
    audio_track_.reset();
    connected_.store(false);
}

}