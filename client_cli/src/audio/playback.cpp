#include "playback.hpp"

#include <iostream>
#include <cstring>

#if defined(__FreeBSD__) || defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#  if defined(__FreeBSD__)
#    include <sys/soundcard.h>
#  else
#    include <linux/soundcard.h>
#  endif
#define HAS_OSS 1
#else
#define HAS_OSS 0
#endif

namespace audio {

Playback::Playback(const std::string& device,
                    uint32_t           sample_rate,
                    uint32_t           frame_ms)
    : device_(device)
    , sample_rate_(sample_rate)
    , frame_ms_(frame_ms)
    , frame_size_(sample_rate * frame_ms / 1000)
    , jitter_buffer_(50, sample_rate)
{}

Playback::~Playback() {
    stop();
}

bool Playback::openDevice() {
#if HAS_OSS
    fd_ = ::open(device_.c_str(), O_WRONLY);
    if (fd_ < 0) {
        std::cerr << "[playback] cannot open " << device_
                  << ": " << strerror(errno) << "\n";
        return false;
    }

    int format = AFMT_S16_LE;
    if (::ioctl(fd_, SNDCTL_DSP_SETFMT, &format) < 0) {
        std::cerr << "[playback] SETFMT failed\n";
        return false;
    }

    int channels = 1;
    if (::ioctl(fd_, SNDCTL_DSP_CHANNELS, &channels) < 0) {
        std::cerr << "[playback] CHANNELS failed\n";
        return false;
    }

    int rate = static_cast<int>(sample_rate_);
    if (::ioctl(fd_, SNDCTL_DSP_SPEED, &rate) < 0) {
        std::cerr << "[playback] SPEED failed\n";
        return false;
    }

    return true;
#else
    std::cerr << "[playback] OSS not supported on this platform\n";
    return false;
#endif
}

void Playback::closeDevice() {
#if HAS_OSS
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
#endif
}

void Playback::start() {
    if (running_.load()) return;
    running_.store(true);
    thread_ = std::thread([this]() { loop(); });
}

void Playback::stop() {
    running_.store(false);
    jitter_buffer_.clear();
    if (thread_.joinable()) thread_.join();
    closeDevice();
}

void Playback::write(const int16_t* data, size_t count) {
    jitter_buffer_.write(data, count);
}

void Playback::playFile(const std::vector<int16_t>& samples) {
    jitter_buffer_.write(samples.data(), samples.size());
}

void Playback::loop() {
    bool has_device = openDevice();

    std::vector<int16_t> frame(frame_size_);

    while (running_.load()) {
        jitter_buffer_.read(frame.data(), frame_size_);

#if HAS_OSS
        if (has_device) {
            ssize_t written = ::write(fd_,
                                       frame.data(),
                                       frame_size_ * sizeof(int16_t));
            if (written < 0) {
                if (running_.load())
                    std::cerr << "[playback] write error\n";
                break;
            }
        } else {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(frame_ms_));
        }
#else
        std::this_thread::sleep_for(
            std::chrono::milliseconds(frame_ms_));
#endif
    }

    if (has_device) closeDevice();
}

}