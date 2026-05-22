#include "wav.hpp"

#include <fstream>
#include <cstring>

namespace audio {

static void writeU32LE(std::ostream& out, uint32_t val) {
    uint8_t buf[4] = {
        static_cast<uint8_t>(val),
        static_cast<uint8_t>(val >> 8),
        static_cast<uint8_t>(val >> 16),
        static_cast<uint8_t>(val >> 24)
    };
    out.write(reinterpret_cast<char*>(buf), 4);
}

static void writeU16LE(std::ostream& out, uint16_t val) {
    uint8_t buf[2] = {
        static_cast<uint8_t>(val),
        static_cast<uint8_t>(val >> 8)
    };
    out.write(reinterpret_cast<char*>(buf), 2);
}

static uint32_t readU32LE(std::istream& in) {
    uint8_t buf[4];
    in.read(reinterpret_cast<char*>(buf), 4);
    return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

static uint16_t readU16LE(std::istream& in) {
    uint8_t buf[2];
    in.read(reinterpret_cast<char*>(buf), 2);
    return buf[0] | (buf[1] << 8);
}

void writeWav(const std::string&          filename,
               const std::vector<int16_t>& samples,
               uint32_t                    sample_rate,
               uint16_t                    num_channels,
               uint16_t                    bits) {
    std::ofstream out(filename, std::ios::binary);
    if (!out)
        throw WavError("cannot open for writing: " + filename);

    uint32_t data_size   = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
    uint32_t byte_rate   = sample_rate * num_channels * (bits / 8);
    uint16_t block_align = num_channels * (bits / 8);

    out.write("RIFF", 4);
    writeU32LE(out, 36 + data_size);    
    out.write("WAVE", 4);

    out.write("fmt ", 4);
    writeU32LE(out, 16);                 
    writeU16LE(out, 1);                  
    writeU16LE(out, num_channels);
    writeU32LE(out, sample_rate);
    writeU32LE(out, byte_rate);
    writeU16LE(out, block_align);
    writeU16LE(out, bits);

    out.write("data", 4);
    writeU32LE(out, data_size);
    out.write(reinterpret_cast<const char*>(samples.data()), data_size);

    if (!out)
        throw WavError("write failed: " + filename);
}

std::vector<int16_t> readWav(const std::string& filename,
                               WavHeader&         header) {
    std::ifstream in(filename, std::ios::binary);
    if (!in)
        throw WavError("cannot open: " + filename);

    char riff[4];
    in.read(riff, 4);
    if (std::strncmp(riff, "RIFF", 4) != 0)
        throw WavError("not a RIFF file: " + filename);

    readU32LE(in);

    char wave[4];
    in.read(wave, 4);
    if (std::strncmp(wave, "WAVE", 4) != 0)
        throw WavError("not a WAVE file: " + filename);

    bool found_fmt  = false;
    bool found_data = false;
    std::vector<int16_t> samples;

    while (in && !found_data) {
        char id[4];
        in.read(id, 4);
        if (!in) break;

        uint32_t chunk_size = readU32LE(in);

        if (std::strncmp(id, "fmt ", 4) == 0) {
            uint16_t format = readU16LE(in);
            if (format != 1)
                throw WavError("only PCM WAV supported: " + filename);

            header.numChannels  = readU16LE(in);
            header.sampleRate   = readU32LE(in);
            readU32LE(in);
            readU16LE(in);
            header.bitsPerSample = readU16LE(in);

            if (header.bitsPerSample != 16)
                throw WavError("only 16-bit WAV supported: " + filename);

            if (chunk_size > 16)
                in.seekg(chunk_size - 16, std::ios::cur);

            found_fmt = true;

        } else if (std::strncmp(id, "data", 4) == 0) {
            if (!found_fmt)
                throw WavError("data chunk before fmt: " + filename);

            samples.resize(chunk_size / sizeof(int16_t));
            in.read(reinterpret_cast<char*>(samples.data()), chunk_size);
            header.numSamples = static_cast<uint32_t>(samples.size());
            found_data = true;

        } else {
            in.seekg(chunk_size, std::ios::cur);
        }
    }

    if (!found_data)
        throw WavError("no data chunk: " + filename);

    return samples;
}

}