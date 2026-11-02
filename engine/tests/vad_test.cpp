#include "audio_studio/vad.hpp"

#include <iostream>
#include <limits>
#include <vector>
#include <cstddef>

namespace {
class FakeStream final : public audio_studio::SileroVadStream {
public:
    bool process_window(const float*, float& p) override {
        p = index++ < 3 ? 0.9F : 0.1F;
        return true;
    }
    void reset() noexcept override { index=0; }
    std::size_t index{0};
};
}


int main() {
    using namespace audio_studio;
    if (kVadSampleRateHz != 16000U || kSileroWindowSamples != 512U ||
        kSileroContextSamples != 64U || kSileroWindowDurationMs != 32U) {
        std::cerr << "Silero stream contract constants changed unexpectedly.\\n";
        return 4;
    }
    const std::vector<VadProbabilityFrame> frames{
        {0, 100, 0.1F}, {100, 200, 0.8F}, {200, 300, 0.9F},
        {300, 400, 0.7F}, {400, 500, 0.1F}, {500, 600, 0.1F},
        {600, 700, 0.9F}, {700, 800, 0.9F}, {800, 900, 0.1F}};
    VadOptions options;
    options.minimum_speech_ms = 150;
    options.minimum_silence_ms = 150;
    options.padding_ms = 50;
    const auto regions = build_speech_regions(frames, options);
    if (regions.size() != 2 || regions[0].start_ms != 50 ||
        regions[0].end_ms != 450 || regions[1].start_ms != 550 ||
        regions[1].end_ms != 850) {
        std::cerr << "Expected two padded speech regions.\n";
        return 1;
    }

    auto invalid = frames;
    invalid[1].speech_probability = std::numeric_limits<float>::quiet_NaN();
    if (!build_speech_regions(invalid, options).empty()) {
        std::cerr << "Invalid probability frames must be rejected.\n";
        return 2;
    }

    options.speech_threshold = 1.5F;
    if (!build_speech_regions(frames, options).empty()) {
        std::cerr << "Invalid options must be rejected.\n";
        return 3;
    }


    FakeStream fake;
    VadOptions streaming_options;
    streaming_options.minimum_speech_ms = 64;
    streaming_options.minimum_silence_ms = 64;
    streaming_options.padding_ms = 16;
    StreamingVadSession session(fake, streaming_options);
    std::vector<SpeechRegion> emitted;
    std::vector<float> audio(512U * 5U, 0.0F);
    if (!session.push_audio(audio.data(), 777U, emitted) || !emitted.empty() ||
        !session.push_audio(audio.data()+777U, audio.size()-777U, emitted) ||
        emitted.size()!=1U || emitted[0].start_ms!=0U || emitted[0].end_ms!=112U) {
        std::cerr << "Incremental VAD should emit a completed padded region.\n";
        return 5;
    }
    if (!session.flush(emitted) || !emitted.empty() || session.flush(emitted)) {
        std::cerr << "Flush should succeed once and reject repeated flush.\n";
        return 6;
    }
    session.reset();
    if (session.audio_duration_ms()!=0U) {
        std::cerr << "Reset should clear streaming timestamps.\n";
        return 7;
    }

    // Invalid chunks must fail without advancing the session timeline.
    session.reset();
    const auto duration_before_invalid = session.audio_duration_ms();
    const float invalid_sample = std::numeric_limits<float>::infinity();
    if (session.push_audio(&invalid_sample, 1U, emitted) ||
        session.audio_duration_ms() != duration_before_invalid ||
        !session.push_audio(nullptr, 0U, emitted)) {
        std::cerr << "Invalid or empty chunks must preserve session state.\\n";
        return 8;
    }

    // A short final speech window is analyzed on flush and bounded by observed audio.
    FakeStream partial_fake;
    VadOptions partial_options = streaming_options;
    partial_options.minimum_speech_ms = 1U;
    StreamingVadSession partial_session(partial_fake, partial_options);
    std::vector<float> partial_audio(256U, 0.0F);
    if (!partial_session.push_audio(partial_audio.data(), partial_audio.size(), emitted) ||
        !emitted.empty() || !partial_session.flush(emitted) ||
        emitted.size() != 1U || emitted[0].start_ms != 0U ||
        emitted[0].end_ms != 16U || partial_session.audio_duration_ms() != 16U) {
        std::cerr << "Flush should analyze and clamp a final partial window.\\n";
        return 9;
    }
    if (partial_session.push_audio(partial_audio.data(), partial_audio.size(), emitted)) {
        std::cerr << "Push after flush must be rejected.\\n";
        return 10;
    }

    std::cout << "VAD region tests passed.\n";
    return 0;
}
