#include "audio_studio/transcription.hpp"
#include "audio_studio/transcription_evaluation.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

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

    const auto insertion = evaluate_transcription(
        "I read a book today.", "I read the book today.");
    if (insertion.reference_words != 5U || insertion.hypothesis_words != 5U ||
        insertion.insertions != 1U || insertion.substitutions != 1U ||
        insertion.deletions != 1U) {
        std::cerr << "Unexpected alignment counts for a mixed recognition error.\n";
        return 6;
    }

    const auto hallucination = evaluate_transcription(
        "The narrator stopped speaking.", "The narrator stopped speaking yesterday.");
    if (hallucination.insertions != 1U || hallucination.deletions != 0U ||
        hallucination.substitutions != 0U || hallucination.word_error_rate() != 0.25) {
        std::cerr << "Unspoken-word insertion was not measured correctly.\n";
        return 7;
    }

    const auto accentSubstitution = evaluate_transcription(
        "Please close the door.", "Please clothes the door.");
    if (accentSubstitution.substitutions != 1U ||
        accentSubstitution.insertions != 0U || accentSubstitution.deletions != 0U) {
        std::cerr << "Similar-sounding word substitution was not measured correctly.\n";
        return 8;
    }

    const auto normalized = evaluate_transcription(
        "THIS, is a test!", "this is a test");
    if (normalized.word_error_rate() != 0.0) {
        std::cerr << "Case and punctuation normalization should not count as errors.\n";
        return 9;
    }

    const auto emptyReference = evaluate_transcription("", "unspoken words");
    if (emptyReference.reference_words != 0U ||
        emptyReference.insertions != 2U || emptyReference.word_error_rate() != 1.0) {
        std::cerr << "Empty-reference insertion handling is invalid.\n";
        return 10;
    }

    std::cout << "Transcription validation and reference-evaluation tests passed.\n";
    return 0;
}
