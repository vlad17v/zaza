#pragma once

#include <vector>
#include <cstdint>
#include <mutex>
#include <cstring>

namespace audio {

class JitterBuffer {
public:
    explicit JitterBuffer(uint32_t capacity_ms = 50,
                          uint32_t sample_rate  = 48000)
        : sample_rate_(sample_rate)
        , capacity_(capacity_ms * sample_rate / 1000)
        , buffer_(capacity_ms * sample_rate / 1000, 0)
        , write_pos_(0)
        , read_pos_(0)
        , available_(0)
    {}

    void write(const int16_t* data, size_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (size_t i = 0; i < count && available_ < capacity_; ++i) {
            buffer_[write_pos_] = data[i];
            write_pos_ = (write_pos_ + 1) % capacity_;
            ++available_;
        }
    }

    size_t read(int16_t* out, size_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t i = 0;
        for (; i < count && available_ > 0; ++i) {
            out[i] = buffer_[read_pos_];
            read_pos_ = (read_pos_ + 1) % capacity_;
            --available_;
        }
        for (; i < count; ++i)
            out[i] = 0;
        return i;
    }

    size_t available() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return available_;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        write_pos_ = read_pos_ = available_ = 0;
        std::fill(buffer_.begin(), buffer_.end(), 0);
    }

private:
    uint32_t             sample_rate_;
    size_t               capacity_;
    std::vector<int16_t> buffer_;
    size_t               write_pos_;
    size_t               read_pos_;
    size_t               available_;
    mutable std::mutex   mutex_;
};

}