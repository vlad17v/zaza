#include "capture.hpp"

#include <iostream>
#include <cstring>
#include <stdexcept>

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

Capture::Capture(const std::string& device,
                  uint32_t           sample_rate,
                  uint32_t           frame_ms)
    : device_(device)
    , sample_rate_(sample_rate)
    , frame_ms_(frame_ms)
    , frame_size_(sample_rate * frame_ms / 1000)
{}

Capture::~Capture() {
    stop();
}

bool Capture::openDevice() {
#if HAS_OSS
    fd_ = ::open(device_.c_str(), O_RDONLY);
    if (fd_ < 0) {
        std::cerr << "[capture] cannot open " << device_
                  << ": " << strerror(errno) << "\n";
        return false;
    }

    int format = AFMT_S16_LE;
    if (::ioctl(fd_, SNDCTL_DSP_SETFMT, &format) < 0) {
        std::cerr << "[capture] SETFMT failed\n";
        return false;
    }

    int channels = 1;
    if (::ioctl(fd_, SNDCTL_DSP_CHANNELS, &channels) < 0) {
        std::cerr << "[capture] CHANNELS failed\n";
        return false;
    }

    int rate = static_cast<int>(sample_rate_);
    if (::ioctl(fd_, SNDCTL_DSP_SPEED, &rate) < 0) {
        std::cerr << "[capture] SPEED failed\n";
        return false;
    }

    return true;
#else
    std::cerr << "[capture] OSS not supported on this platform\n";
    return false;
#endif
}

void Capture::closeDevice() {
#if HAS_OSS
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
#endif
}

void Capture::start(AudioCallback callback) {
    if (running_.load()) return;
    callback_ = std::move(callback);
    running_.store(true);
    thread_ = std::thread([this]() { loop(); });
}

void Capture::stop() {
    running_.store(false);
    if (thread_.joinable()) thread_.join();
    closeDevice();
}

void Capture::startRecording() {
    std::lock_guard<std::mutex> lock(record_mutex_);
    record_buf_.clear();
    recording_.store(true);
}

std::vector<int16_t> Capture::stopRecording() {
    recording_.store(false);
    std::lock_guard<std::mutex> lock(record_mutex_);
    return std::move(record_buf_);
}

void Capture::loop() {
    bool has_device = openDevice();

    std::vector<int16_t> frame(frame_size_);

    while (running_.load()) {
#if HAS_OSS
        if (has_device) {
            ssize_t bytes = ::read(fd_,
                                   frame.data(),
                                   frame_size_ * sizeof(int16_t));
            if (bytes <= 0) {
                if (running_.load())
                    std::cerr << "[capture] read error\n";
                break;
            }

            size_t samples = bytes / sizeof(int16_t);

            if (recording_.load()) {
                std::lock_guard<std::mutex> lock(record_mutex_);
                record_buf_.insert(record_buf_.end(),
                                   frame.begin(),
                                   frame.begin() + samples);
            }

            if (!muted_.load() && callback_) {
                callback_(frame.data(), samples);
            } else if (callback_) {
                std::vector<int16_t> silence(samples, 0);
                callback_(silence.data(), samples);
            }
        } else {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(frame_ms_));
            if (callback_) {
                std::vector<int16_t> silence(frame_size_, 0);
                callback_(silence.data(), frame_size_);
            }
        }
#else
        std::this_thread::sleep_for(
            std::chrono::milliseconds(frame_ms_));
        if (callback_) {
            std::vector<int16_t> silence(frame_size_, 0);
            callback_(silence.data(), frame_size_);
        }
#endif
    }

    if (has_device) closeDevice();
}

}