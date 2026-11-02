#include "audio_studio/speech_region_delivery.hpp"

#include <iostream>
#include <vector>

int main() {
    using namespace audio_studio;
    const std::vector<SpeechRegion> queued{{120, 640}, {900, 1240}};
    SpeechRegion output[2]{{7, 8}, {9, 10}};
    std::size_t written = 99;

    if (copy_speech_regions(queued, output, 1, &written) || written != 2 ||
        output[0].start_ms != 7 || output[0].end_ms != 8 ||
        output[1].start_ms != 9 || output[1].end_ms != 10) {
        std::cerr << "Insufficient capacity must report count without partial copy.\\n";
        return 1;
    }
    if (!copy_speech_regions(queued, output, 2, &written) || written != 2 ||
        output[0].start_ms != 120 || output[0].end_ms != 640 ||
        output[1].start_ms != 900 || output[1].end_ms != 1240) {
        std::cerr << "Retry with sufficient capacity must copy all queued regions.\\n";
        return 2;
    }
    if (!copy_speech_regions({}, nullptr, 0, &written) || written != 0) {
        std::cerr << "Empty delivery must succeed with zero capacity.\\n";
        return 3;
    }
    if (copy_speech_regions(queued, output, 2, nullptr)) {
        std::cerr << "Missing written-count pointer must fail.\\n";
        return 4;
    }
    return 0;
}
