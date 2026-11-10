#include "audio_studio/transcription.hpp"

#include "whisper.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <thread>

namespace audio_studio {
namespace {
TranscriptionResult failure(TranscriptionError error, const char* message) {
    TranscriptionResult result;
    result.error = error;
    result.error_message = message;
    return result;
}
}

TranscriptionResult validate_transcription_request(const TranscriptionRequest& request) {

    if (request.model_path.empty() || request.mono_pcm_16khz.empty()) {
        return failure(TranscriptionError::InvalidRequest,
                       "A local model path and non-empty 16 kHz mono PCM are required.");
    }
    if (!std::all_of(request.mono_pcm_16khz.begin(), request.mono_pcm_16khz.end(),
                     [](float sample) { return std::isfinite(sample); })) {
        return failure(TranscriptionError::InvalidRequest,
                       "PCM input contains a non-finite sample.");
    }
    if (request.mono_pcm_16khz.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return failure(TranscriptionError::InvalidRequest, "PCM input is too large.");
    }
    return {};
}

TranscriptionResult transcribe(const TranscriptionRequest& request) {
    const TranscriptionResult validation = validate_transcription_request(request);
    if (!validation) {
        return validation;
    }
    std::error_code filesystem_error;
    if (!std::filesystem::is_regular_file(request.model_path, filesystem_error)) {
        return failure(TranscriptionError::ModelNotFound,
                       "The bundled Whisper model file was not found.");
    }

    whisper_context_params context_params = whisper_context_default_params();
    context_params.use_gpu = false;
    whisper_context* context =
        whisper_init_from_file_with_params(request.model_path.c_str(), context_params);
    if (context == nullptr) {
        return failure(TranscriptionError::ModelLoadFailed,
                       "Whisper.cpp could not load the local model.");
    }

    const bool automatic_language = request.language.empty() || request.language == "auto";
    if (!automatic_language && whisper_lang_id(request.language.c_str()) < 0) {
        whisper_free(context);
        return failure(TranscriptionError::InvalidRequest,
                       "The requested language is not supported by Whisper.");
    }

    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    const unsigned int available_threads = std::thread::hardware_concurrency();
    // Windows hosted runners can stall in the multi-threaded CPU inference path.
    // Keep Windows inference deterministic with one worker; other platforms retain
    // the bounded parallel path for better throughput.
#if defined(_WIN32)
    params.n_threads = 1;
#else
    // Avoid oversubscribing hosted CI and low-core devices.
    params.n_threads = static_cast<int>(std::clamp(available_threads == 0U ? 2U : available_threads, 1U, 4U));
#endif
    params.translate = request.translate_to_english;
    params.no_context = true;
    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.language = automatic_language ? "auto" : request.language.c_str();
    // "auto" already requests language detection. Setting detect_language=true
    // makes whisper_full return immediately after detection without decoding speech.
    params.detect_language = false;

    const int status = whisper_full(context, params, request.mono_pcm_16khz.data(),
                                    static_cast<int>(request.mono_pcm_16khz.size()));
    if (status != 0) {
        whisper_free(context);
        return failure(TranscriptionError::InferenceFailed,
                       "Whisper.cpp inference failed.");
    }

    TranscriptionResult result;
    const int language_id = whisper_full_lang_id(context);
    const char* language = whisper_lang_str(language_id);
    if (language != nullptr) {
        result.detected_language = language;
    }

    const int segment_count = whisper_full_n_segments(context);
    for (int index = 0; index < segment_count; ++index) {
        const char* segment_text = whisper_full_get_segment_text(context, index);
        const int64_t start = std::max<int64_t>(0, whisper_full_get_segment_t0(context, index));
        const int64_t end = std::max(start, whisper_full_get_segment_t1(context, index));
        TranscriptionSegment segment;
        segment.start_ms = static_cast<std::uint64_t>(start) * 10U;
        segment.end_ms = static_cast<std::uint64_t>(end) * 10U;
        if (segment_text != nullptr) {
            segment.text = segment_text;
            result.text += segment.text;
        }
        result.segments.push_back(std::move(segment));
    }

    whisper_free(context);
    return result;
}
}  // namespace audio_studio
