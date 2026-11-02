#pragma once

#include <cstddef>
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

// Silero processes fixed 512-sample windows at 16 kHz with a rolling
// 64-sample context and recurrent state owned by each independent stream.
inline constexpr std::uint32_t kSileroWindowSamples = 512U;
inline constexpr std::uint32_t kSileroContextSamples = 64U;
inline constexpr std::uint32_t kSileroWindowDurationMs = 32U;

struct SpeechRegion {
    std::uint64_t start_ms{0U};
    std::uint64_t end_ms{0U};
};

// Converts ordered probability frames into padded speech regions. Invalid
// options/frames produce an empty result; regions are clamped to frame bounds.
// Platform ONNX Runtime adapters implement this stateful inference boundary.
// Call reset() before each independent recording to prevent state leakage.
class SileroVadStream {
public:
    virtual ~SileroVadStream() = default;
    virtual bool process_window(const float* mono_pcm_512,
                               float& speech_probability) = 0;
    virtual void reset() noexcept = 0;
};


// Incremental coordinator over a caller-owned inference stream. push_audio()
// emits only regions whose trailing silence has been confirmed; flush() closes
// any remaining region and analyzes a final partial window padded with zeros.
// Call reset() between independent recordings.
class StreamingVadSession {
public:
    explicit StreamingVadSession(SileroVadStream& stream,
                                VadOptions options = {});
    bool push_audio(const float* mono_pcm, std::size_t sample_count,
                    std::vector<SpeechRegion>& completed_regions);
    bool flush(std::vector<SpeechRegion>& completed_regions);
    void reset() noexcept;
    std::uint64_t audio_duration_ms() const noexcept;

private:
    bool analyze_window(std::size_t valid_samples,
                        std::vector<SpeechRegion>& completed_regions);
    void consume_probability(float probability, std::uint64_t start_ms,
                             std::uint64_t end_ms,
                             std::vector<SpeechRegion>& completed_regions);
    void finalize_region(std::uint64_t observed_end_ms,
                         std::vector<SpeechRegion>& completed_regions);

    SileroVadStream& stream_;
    VadOptions options_;
    std::vector<float> pending_;
    std::uint64_t samples_seen_{0U};
    bool active_{false};
    std::uint64_t speech_start_ms_{0U};
    std::uint64_t last_speech_end_ms_{0U};
    bool flushed_{false};
    bool failed_{false};
};

std::vector<SpeechRegion> build_speech_regions(
    const std::vector<VadProbabilityFrame>& frames,
    const VadOptions& options = {});

}  // namespace audio_studio
