#include "audio_studio/vad.hpp"

#include <iostream>
#include <limits>
#include <vector>

int main() {
    using namespace audio_studio;
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

    std::cout << "VAD region tests passed.\n";
    return 0;
}
