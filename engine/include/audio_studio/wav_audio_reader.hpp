#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace audio_studio {
// Decodes supported WAV input in bounded blocks, downmixes and resamples to
// normalized mono 16 kHz. The final PCM vector is required by Whisper inference.
bool load_wav_for_transcription(const std::string& path,
                                std::vector<float>& mono_pcm_16khz,
                                std::string* error = nullptr,
                                std::uint64_t max_output_samples = 16ULL * 60ULL * 60ULL * 16000ULL);
} // namespace audio_studio
