#include "audio_studio/transcription.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>

int main() {
    using namespace audio_studio;

    // Use explicit checks rather than assert: Release builds define NDEBUG,
    // which otherwise removes the assertions and makes this test ineffective.
    TranscriptionRequest request;
    auto result = transcribe(request);
    if (result.error != TranscriptionError::InvalidRequest || result ||
        !result.segments.empty()) {
        std::cerr << "Empty request must be rejected before touching the model.\n";
        return 1;
    }

    request.model_path =
        (std::filesystem::temp_directory_path() /
         "synvidra-transcription-test-model-that-does-not-exist.bin").string();
    request.mono_pcm_16khz = {0.0F};
    result = transcribe(request);
    if (result.error != TranscriptionError::ModelNotFound) {
        std::cerr << "A missing model must return ModelNotFound.\n";
        return 2;
    }

    request.mono_pcm_16khz = {std::numeric_limits<float>::quiet_NaN()};
    result = transcribe(request);
    if (result.error != TranscriptionError::InvalidRequest) {
        std::cerr << "Non-finite PCM must be rejected before model lookup.\n";
        return 3;
    }

    if (kTranscriptionSampleRateHz != 16000U ||
        std::string(kDefaultTranscriptionModel).empty()) {
        std::cerr << "Transcription defaults are invalid.\n";
        return 4;
    }

    std::cout << "Transcription request validation passed.\n";
    return 0;
}
