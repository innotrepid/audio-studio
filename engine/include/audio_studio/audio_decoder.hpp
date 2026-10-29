#pragma once

#include "audio_studio/audio_types.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace audio_studio {

// Format-independent, bounded-memory source of normalized interleaved float frames.
// Implementations must leave destination samples untouched beyond frames_read.
class AudioDecoder {
public:
    virtual ~AudioDecoder() = default;
    [[nodiscard]] virtual AudioFormat format() const noexcept = 0;
    [[nodiscard]] virtual std::uint64_t total_frames() const noexcept = 0;
    [[nodiscard]] virtual std::uint64_t position_frames() const noexcept = 0;
    [[nodiscard]] virtual bool seek_frame(std::uint64_t frame, std::string* error = nullptr) = 0;
    [[nodiscard]] virtual bool read_frames(float* destination, std::size_t requested_frames,
                                           std::size_t& frames_read,
                                           std::string* error = nullptr) = 0;
};

// Opens supported audio input using a format-specific decoder. The first backend
// supports RIFF/WAVE PCM 8/16/24/32-bit and IEEE float32 mono/stereo.
[[nodiscard]] std::unique_ptr<AudioDecoder> open_audio_decoder(
    const std::string& path, std::string* error = nullptr);

}  // namespace audio_studio
