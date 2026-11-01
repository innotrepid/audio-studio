#pragma once

#include <cstdint>
#include <vector>

namespace audio_studio {

// Silero consumes mono float PCM at 16 kHz. Runtime adapters return one
// probability frame per analysis window; this layer turns them into stable
// speech regions without depending on ONNX Runtime.
inline constexpr std::uint32_t kVadSampleRateHz = 16000U;

struct VadProbabilityFrame {
    std::uint64_t start_ms{0U};
    std::uint64_t end_ms{0U};
    float speech_probability{0.0F};
};

struct VadOptions {
    float speech_threshold{0.5F};
    std::uint64_t minimum_speech_ms{100U};
    std::uint64_t minimum_silence_ms{300U};
    std::uint64_t padding_ms{80U};
};

struct SpeechRegion {
    std::uint64_t start_ms{0U};
    std::uint64_t end_ms{0U};
};

// Converts ordered probability frames into padded speech regions. Invalid
// options/frames produce an empty result; regions are clamped to frame bounds.
std::vector<SpeechRegion> build_speech_regions(
    const std::vector<VadProbabilityFrame>& frames,
    const VadOptions& options = {});

}  // namespace audio_studio
