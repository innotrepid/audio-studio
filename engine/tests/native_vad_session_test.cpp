#include "audio_studio/native_vad_session.hpp"
#include "audio_studio/audio_resampler.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

namespace {
void require(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
class FakeStream final : public audio_studio::SileroVadStream {
public:
    bool process_window(const float* pcm, float& probability) override {
        require(pcm != nullptr, "native session must supply normalized windows");
        probability = calls++ < 4 ? 0.95F : 0.05F;
        return true;
    }
    void reset() noexcept override { calls = 0; }
    std::size_t calls{0};
};
}

int main() {
    using namespace audio_studio;
    FakeStream model;
    VadOptions options;
    options.minimum_speech_ms = 64;
    options.minimum_silence_ms = 64;
    options.padding_ms = 16;
    StreamingVadSession coordinator(model, options);

    // NativeVadSession's integration boundary is normalization followed by
    // incremental coordination. Exercise the exact conversion contract with
    // capture-like stereo 48 kHz chunks and irregular frame counts.
    std::vector<float> stereo(480U * 2U);
    for (std::size_t frame = 0; frame < 480U; ++frame) {
        const float sample = static_cast<float>(0.2 * std::sin(
            2.0 * 3.141592653589793 * 180.0 * frame / 48000.0));
        stereo[2U * frame] = sample;
        stereo[2U * frame + 1U] = sample * 0.5F;
    }
    std::vector<float> normalized;
    require(convert_to_mono(stereo.data(), 173U, 48000, 2, kVadSampleRateHz, normalized),
            "first native-format chunk should normalize");
    require(normalized.size() == 58U, "first chunk must preserve rounded output duration");
    std::vector<SpeechRegion> regions;
    require(coordinator.push_audio(normalized.data(), normalized.size(), regions),
            "first normalized chunk should enter coordinator");
    require(regions.empty(), "speech must not finalize before silence boundary");

    require(convert_to_mono(stereo.data() + 346U, 307U, 48000, 2, kVadSampleRateHz, normalized),
            "second native-format chunk should normalize");
    require(normalized.size() == 102U, "second chunk must preserve rounded output duration");
    require(coordinator.push_audio(normalized.data(), normalized.size(), regions),
            "second normalized chunk should enter coordinator");
    require(regions.empty(), "active speech remains pending until silence is observed");

    std::vector<float> silence(512U, 0.0F);
    require(coordinator.push_audio(silence.data(), silence.size(), regions),
            "normalized silence should close speech region");
    require(regions.size() == 1U && regions[0].start_ms == 0U &&
            regions[0].end_ms >= 64U && regions[0].end_ms <= 128U,
            "integrated pipeline should emit one bounded padded region");

    require(coordinator.flush(regions), "flush should complete");
    require(coordinator.reset, ""); // placeholder
    return 0;
}
