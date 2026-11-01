#include "audio_studio/transcription.hpp"

#include <cmath>
#include <iostream>
#include <limits>

int main() {
    using namespace audio_studio;
    TranscriptionRequest request;
    auto result = validate_transcription_request(request);
    if (result.error != TranscriptionError::InvalidRequest || result) {
        std::cerr << "Empty request must be rejected.\n";
        return 1;
    }

    request.model_path = "bundled-model.bin";
    request.mono_pcm_16khz = {0.0F};
    result = validate_transcription_request(request);
    if (result.error != TranscriptionError::None || !result) {
        std::cerr << "Finite non-empty PCM request must pass validation.\n";
        return 2;
    }

    request.mono_pcm_16khz = {std::numeric_limits<float>::quiet_NaN()};
    result = validate_transcription_request(request);
    if (result.error != TranscriptionError::InvalidRequest) {
        std::cerr << "Non-finite PCM must be rejected.\n";
        return 3;
    }

    request.mono_pcm_16khz = {std::numeric_limits<float>::infinity()};
    result = validate_transcription_request(request);
    if (result.error != TranscriptionError::InvalidRequest) {
        std::cerr << "Infinite PCM must be rejected.\n";
        return 4;
    }

    if (kTranscriptionSampleRateHz != 16000U ||
        std::string(kDefaultTranscriptionModel).empty()) {
        std::cerr << "Transcription defaults are invalid.\n";
        return 5;
    }
    std::cout << "Transcription request validation passed.\n";
    return 0;
}
