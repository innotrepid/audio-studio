#pragma once

#include "audio_studio/audio_types.hpp"

#include <string>

namespace audio_studio {

// Reads little-endian integer PCM16 WAV files into normalized interleaved floats.
// Supports mono/stereo PCM and arbitrary valid RIFF chunk order/padding. The
// destination is unchanged if parsing or decoding fails.
class WavReader final {
public:
    [[nodiscard]] static bool read(const std::string& path, AudioBuffer& destination,
                                   std::string* error = nullptr);
};

}  // namespace audio_studio
