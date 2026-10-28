#pragma once

#include "audio_studio/noise_gate.hpp"
#include "audio_studio/dsp_effects.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

namespace audio_studio {

// Streams PCM16 WAV audio through the gate in bounded blocks and writes a
// separate PCM16 WAV. Memory use is independent of recording duration; the
// source file is never modified.
[[nodiscard]] bool process_wav_with_noise_gate(
    const std::string& input_path,
    const std::string& output_path,
    NoiseGateSettings settings = {},
    std::string* error = nullptr,
    const std::atomic<bool>* cancel_requested = nullptr,
    std::function<void(std::uint64_t, std::uint64_t)> progress = {},
    DspEffectsSettings dsp_settings = {});

}  // namespace audio_studio
