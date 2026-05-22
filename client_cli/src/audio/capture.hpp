#pragma once

#include "jitter_buffer.hpp"

#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <vector>
#include <cstdint>

namespace audio {

using AudioCallback = std::function<void(const int16_t* data, size_t count)>;

class Capture {
public:
    explicit Capture(const std::string& device      = "/dev/dsp",
                     uint32_t           sample_rate = 48000,
                     uint32_t           frame_ms    = 20);

    ~Capture();

    void start(AudioCallback callback);

    void stop();

    void setMuted(bool muted) { muted_.store(muted); }
    bool isMuted() const      { return muted_.load(); }

    void startRecording();

    std::vector<int16_t> stopRecording();

    bool isRunning() const { return running_.load(); }

private:
    void loop();
    bool openDevice();
    void closeDevice();

    std::string   device_;
    uint32_t      sample_rate_;
    uint32_t      frame_ms_;
    uint32_t      frame_size_;

    int           fd_         = -1;
    std::thread   thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> muted_{false};

    AudioCallback callback_;

    std::atomic<bool>    recording_{false};
    std::vector<int16_t> record_buf_;
    std::mutex           record_mutex_;
};

}