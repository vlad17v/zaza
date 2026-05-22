#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace audio {

struct WavError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct WavHeader {
    uint32_t sampleRate;
    uint16_t numChannels;
    uint16_t bitsPerSample;
    uint32_t numSamples;
};

void writeWav(const std::string&           filename,
              const std::vector<int16_t>&  samples,
              uint32_t                     sample_rate  = 48000,
              uint16_t                     num_channels = 1,
              uint16_t                     bits         = 16);

std::vector<int16_t> readWav(const std::string& filename,
                              WavHeader&         header);

}