#pragma once

#include "audio_studio/noise_gate.hpp"
#include "audio_studio/dsp_effects.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace audio_studio {

struct WaveformPeak {
    float min_left = 0.0F;
    float max_left = 0.0F;
    float min_right = 0.0F;
    float max_right = 0.0F;
};

// Returns a bounded min/max waveform envelope without loading the full audio
// file into memory. Mono files duplicate the left channel into the right fields.
[[nodiscard]] bool analyze_wav_waveform(
    const std::string& input_path,
    std::size_t bucket_count,
    std::vector<WaveformPeak>& peaks,
    AudioFormat* format = nullptr,
    std::uint64_t* total_frames = nullptr,
    std::string* error = nullptr);

// Writes the half-open frame range [start_frame, end_frame) to a separate WAV.
// The source is never modified and memory use is bounded independently of duration.
[[nodiscard]] bool trim_wav(
    const std::string& input_path,
    const std::string& output_path,
    std::uint64_t start_frame,
    std::uint64_t end_frame,
    std::string* error = nullptr);

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
