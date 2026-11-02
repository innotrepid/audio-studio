#include "audio_studio/silero_onnx.hpp"

#include <cmath>
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
