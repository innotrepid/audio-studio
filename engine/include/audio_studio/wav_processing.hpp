#pragma once

#include "audio_studio/noise_gate.hpp"

#include <string>

namespace audio_studio {

// Reads a PCM16 WAV, applies the gate to decoded samples, and writes a separate
// PCM16 WAV. The source file is never modified. Large files are bounded to
// keep whole-file decoding from exhausting mobile memory.
[[nodiscard]] bool process_wav_with_noise_gate(
    const std::string& input_path,
    const std::string& output_path,
    NoiseGateSettings settings = {},
    std::string* error = nullptr);

}  // namespace audio_studio
