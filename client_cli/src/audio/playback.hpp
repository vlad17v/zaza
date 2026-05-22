#pragma once

#include "jitter_buffer.hpp"

#include <string>
#include <thread>
#include <atomic>
#include <cstdint>
#include <vector>

namespace audio {

class Playback {
public:
    explicit Playback(const std::string& device      = "/dev/dsp",
                      uint32_t           sample_rate = 48000,
                      uint32_t           frame_ms    = 20);

    ~Playback();

    void start();
    void stop();

    void write(const int16_t* data, size_t count);

    void playFile(const std::vector<int16_t>& samples);

    bool isRunning() const { return running_.load(); }

private:
    void loop();
    bool openDevice();
    void closeDevice();

    std::string       device_;
    uint32_t          sample_rate_;
    uint32_t          frame_ms_;
    uint32_t          frame_size_;

    int               fd_      = -1;
    std::thread       thread_;
    std::atomic<bool> running_{false};

    JitterBuffer      jitter_buffer_;
};

}