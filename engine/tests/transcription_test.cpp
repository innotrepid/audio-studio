#include "audio_studio/transcription.hpp"

#include <cassert>
#include <limits>

int main() {
    using namespace audio_studio;

    TranscriptionRequest request;
    auto result = transcribe(request);
    assert(result.error == TranscriptionError::InvalidRequest);
    assert(!result);
    assert(result.segments.empty());

    request.model_path = "missing-model.bin";
    request.mono_pcm_16khz = {0.0F};
    result = transcribe(request);
    assert(result.error == TranscriptionError::ModelNotFound);

    request.mono_pcm_16khz = {std::numeric_limits<float>::quiet_NaN()};
    result = transcribe(request);
    assert(result.error == TranscriptionError::InvalidRequest);

    assert(kTranscriptionSampleRateHz == 16000U);
    assert(kDefaultTranscriptionModel != nullptr);
    return 0;
}
