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
        ++calls;
        if (fail_next) { fail_next = false; return false; }
        probability = calls <= 4 ? 0.95F : 0.05F;
        return true;
    }
    void reset() noexcept override { calls = 0; ++resets; }
    bool fail_next{false};
    std::size_t calls{0};
    std::size_t resets{0};
};
}

int main() {
    using namespace audio_studio;
    auto model = std::make_unique<FakeStream>();
    VadOptions options;
    options.minimum_speech_ms = 64;
    options.minimum_silence_ms = 64;
    options.padding_ms = 16;
    auto session = NativeVadSession::create_for_testing(48000, 2, std::move(model), options);
    require(session != nullptr, "test session should construct with injected model");

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
    require(session->push_audio(stereo.data(), 173U, regions),
            "first normalized chunk should enter coordinator");
    require(regions.empty(), "speech must not finalize before silence boundary");

    require(convert_to_mono(stereo.data() + 346U, 307U, 48000, 2, kVadSampleRateHz, normalized),
            "second native-format chunk should normalize");
    require(normalized.size() == 102U, "second chunk must preserve rounded output duration");
    require(session->push_audio(stereo.data() + 346U, 307U, regions),
            "second normalized chunk should enter coordinator");
    require(regions.empty(), "active speech remains pending until silence is observed");

    // 12,288 capture frames at 48 kHz produce eight 512-sample VAD windows
    // at 16 kHz, enough for the fake model's speech-to-silence transition.
    std::vector<float> silence(12288U * 2U, 0.0F);
    require(session->push_audio(silence.data(), 12288U, regions),
            "normalized silence should close speech region");
    require(regions.size() == 1U && regions[0].start_ms == 0U &&
            regions[0].end_ms >= 64U && regions[0].end_ms <= 192U,
            "integrated pipeline should emit one bounded padded region");

    require(session->push_audio(silence.data(), 512U, regions),
            "continued silence should be accepted after region finalization");
    require(regions.empty(), "finalized regions must not be emitted more than once");
    require(session->flush(regions), "flush should complete");
    require(!session->flush(regions), "a second flush must not finalize the stream twice");
    require(!session->push_audio(silence.data(), 12288U, regions),
            "push after flush must be rejected");
    session->reset();
    require(session != nullptr, "reset must retain a valid session");
    require(session->push_audio(silence.data(), 256U, regions),
            "reset must reopen integrated stream");
    require(session->flush(regions), "fresh stream must flush");

    auto failing_model = std::make_unique<FakeStream>();
    FakeStream* failing_model_ptr = failing_model.get();
    failing_model_ptr->fail_next = true;
    auto failing_session = NativeVadSession::create_for_testing(16000, 1,
        std::move(failing_model), options);
    require(failing_session != nullptr, "failure test session should construct");
    std::vector<float> one_window(kSileroWindowSamples, 0.0F);
    require(!failing_session->push_audio(one_window.data(), one_window.size(), regions),
            "inference failure must propagate through NativeVadSession");
    require(!failing_session->push_audio(one_window.data(), one_window.size(), regions),
            "failed coordinator must reject further audio until reset");
    failing_session->reset();
    require(failing_model_ptr->resets == 1U,
            "reset must reset the injected inference stream");
    require(failing_session->push_audio(one_window.data(), one_window.size(), regions),
            "reset must recover the inference pipeline after model failure");
    require(failing_session->flush(regions), "recovered stream should flush");
    return 0;
}
