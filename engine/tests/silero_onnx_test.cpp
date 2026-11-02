#include "audio_studio/silero_onnx.hpp"
#include "audio_studio/native_api.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    using namespace audio_studio;
    if (argc != 2) {
        std::cerr << "Expected path to the pinned Silero ONNX model.\n";
        return 1;
    }

    // Validate C ABI argument handling before loading the real model.
    size_t written = 99;
    if (audio_studio_vad_push_audio(nullptr, nullptr, 0, nullptr, 0, &written) != 0 ||
        written != 0) {
        std::cerr << "A null VAD handle must fail and zero written.\n";
        return 6;
    }
    if (audio_studio_vad_flush(nullptr, nullptr, 0, &written) != 0 ||
        written != 0) {
        std::cerr << "Flushing a null VAD handle must fail and zero written.\n";
        return 7;
    }
    if (audio_studio_vad_reset(nullptr) != 0) {
        std::cerr << "Resetting a null VAD handle must fail.\n";
        return 8;
    }
    if (audio_studio_vad_create(L"invalid-threshold-test.onnx", 16000, 1,
                                std::numeric_limits<float>::quiet_NaN(), 0, 0, 0) != nullptr ||
        std::string(audio_studio_vad_last_error()).empty()) {
        std::cerr << "An invalid threshold must fail with a diagnostic.\n";
        return 9;
    }

    std::string error;
    auto missing = OnnxSileroVadStream::create(
        "missing-silero-model-for-test.onnx", error);
    if (missing || error.empty()) {
        std::cerr << "A missing model must fail with a diagnostic.\n";
        return 2;
    }

    auto stream = OnnxSileroVadStream::create(argv[1], error);
    if (!stream || !error.empty()) {
        std::cerr << "Could not initialize the pinned Silero model: " << error << "\n";
        return 3;
    }

    float probability = 0.375F;
    if (stream->process_window(nullptr, probability) || probability != 0.375F) {
        std::cerr << "Null audio must be rejected without modifying output.\n";
        return 4;
    }

    std::vector<float> samples(kSileroWindowSamples, 0.0F);
    samples[17] = std::numeric_limits<float>::quiet_NaN();
    if (stream->process_window(samples.data(), probability) || probability != 0.375F) {
        std::cerr << "Non-finite audio must be rejected without modifying output.\n";
        return 5;
    }

    stream->reset();
    stream->reset();
    std::cout << "Silero ONNX adapter validation tests passed.\n";
    return 0;
}
