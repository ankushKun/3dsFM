#pragma once

#include "tags.hpp"

#include <cstdint>
#include <memory>
#include <string>

// One decoder per format, all producing interleaved 16-bit stereo PCM at the
// file's own sample rate (Opus always 48 kHz).
namespace audio {

class Decoder {
public:
    virtual ~Decoder() = default;
    virtual bool open(const std::string &path, std::string &error) = 0;
    virtual int sampleRate() const = 0;
    // Fills `frames` stereo frames; returns how many were written, 0 at the end.
    virtual size_t read(int16_t *out, size_t frames) = 0;
    virtual bool seek(double seconds) = 0;
    // Seconds decoded so far.
    virtual double position() const = 0;
};

std::unique_ptr<Decoder> makeDecoder(core::Format format);

} // namespace audio
