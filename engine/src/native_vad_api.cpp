#include "audio_studio/native_api.h"
#include "audio_studio/native_vad_session.hpp"

#include <cmath>
#include <memory>
#include <string>
#include <vector>
#ifdef AUDIO_STUDIO_NATIVE_VAD_TESTING
#include "native_vad_api_test_seam.hpp"
#endif

#ifdef _WIN32
#include <filesystem>
#endif

struct AudioStudioVadHandle {
    std::unique_ptr<audio_studio::NativeVadSessionInterface> session;
    std::vector<audio_studio::SpeechRegion> pending_regions;
    bool flushed{false};
    bool push_retry_pending{false};
};

namespace {
thread_local std::string vad_error;

void clear_error() { vad_error.clear(); }
void set_error(const char* message) { vad_error = message ? message : "VAD operation failed."; }

// Pure delivery helper: does not mutate the queued regions. This makes the
// required-capacity/retry contract deterministic to test without model audio.
int copy_regions(const std::vector<audio_studio::SpeechRegion>& regions,
                 AudioStudioSpeechRegion* output, size_t capacity,
                 size_t* written) {
    if (!written) {
        set_error("The written-count pointer is required.");
        return 0;
    }
    *written = regions.size();
    if (regions.size() > capacity || (!regions.empty() && !output)) {
        set_error("Output capacity is insufficient; no regions were copied.");
        return 0;
    }
    for (size_t i = 0; i < regions.size(); ++i) {
        output[i].start_ms = regions[i].start_ms;
        output[i].end_ms = regions[i].end_ms;
    }
    return 1;
}
}

extern "C" {

// Creates an independent VAD stream. On Windows, model_path is a NUL-terminated
// UTF-16 path. Audio is normalized float32 PCM interleaved by channel; frame_count
// counts frames, not individual channel samples. Calls on a handle must be
// serialized by the caller. Returns nullptr on failure; query the thread-local
// error immediately with audio_studio_vad_last_error().
AudioStudioVadHandle* audio_studio_vad_create(
    const wchar_t* model_path, uint32_t sample_rate, uint32_t channels,
    float speech_threshold, uint64_t minimum_speech_ms,
    uint64_t minimum_silence_ms, uint64_t padding_ms) {
    clear_error();
    if (!model_path || !*model_path || !std::isfinite(speech_threshold) ||
        speech_threshold < 0.0F || speech_threshold > 1.0F) {
        set_error("Invalid model path or speech threshold.");
        return nullptr;
    }
    try {
        std::string path;
#if defined(AUDIO_STUDIO_NATIVE_VAD_TESTING)
        path = "test-session";
#elif defined(_WIN32)
        path = std::filesystem::path(model_path).u8string();
#else
        set_error("Native Silero VAD is supported only by the Windows native build.");
        return nullptr;
#endif
        audio_studio::VadOptions options;
        options.speech_threshold = speech_threshold;
        options.minimum_speech_ms = minimum_speech_ms;
        options.minimum_silence_ms = minimum_silence_ms;
        options.padding_ms = padding_ms;
        std::string error;
#ifdef AUDIO_STUDIO_NATIVE_VAD_TESTING
        auto session = audio_studio::native_vad_test::create_session(path, sample_rate, channels, options, error);
#else
        auto session = audio_studio::NativeVadSession::create(path, sample_rate, channels, options, error);
#endif
        if (!session) {
            set_error(error.c_str());
            return nullptr;
        }
        auto* handle = new AudioStudioVadHandle;
        handle->session = std::move(session);
        return handle;
    } catch (const std::exception& e) {
        set_error(e.what());
    } catch (...) {
        set_error("Could not allocate native VAD handle.");
    }
    return nullptr;
}

void audio_studio_vad_destroy(AudioStudioVadHandle* handle) {
    clear_error();
    delete handle;
}

// Returns 1 on success, 0 on failure. On output-capacity failure, *written
// reports the required region count and queued regions remain retained. Retry
// with a larger output buffer; do not replay the same audio chunk.
int audio_studio_vad_push_audio(
    AudioStudioVadHandle* handle, const float* interleaved, size_t frame_count,
    AudioStudioSpeechRegion* output, size_t capacity, size_t* written) {
    clear_error();
    if (!written) {
        set_error("The written-count pointer is required.");
        return 0;
    }
    *written = 0;
    if (!handle || !handle->session || (!interleaved && frame_count != 0)) {
        set_error("Invalid VAD handle or audio buffer.");
        if (written) *written = 0;
        return 0;
    }
    try {
        if (handle->flushed) {
            set_error("VAD stream has been flushed; reset before pushing more audio.");
            return 0;
        }
        if (handle->push_retry_pending) {
            if (!copy_regions(handle->pending_regions, output, capacity, written)) return 0;
            handle->pending_regions.clear();
            handle->push_retry_pending = false;
            return 1;
        }
        std::vector<audio_studio::SpeechRegion> regions;
        if (!handle->session->push_audio(interleaved, frame_count, regions)) {
            set_error("VAD audio processing failed; reset the session before retrying.");
            if (written) *written = 0;
            return 0;
        }
        handle->pending_regions.insert(handle->pending_regions.end(), regions.begin(), regions.end());
        if (!copy_regions(handle->pending_regions, output, capacity, written)) {
            handle->push_retry_pending = true;
            return 0;
        }
        handle->pending_regions.clear();
        return 1;
    } catch (const std::exception& e) {
        set_error(e.what());
    } catch (...) {
        set_error("Unexpected error while processing VAD audio.");
    }
    if (written) *written = 0;
    return 0;
}

// Finalizes the stream once. If output capacity is insufficient, final regions
// remain queued; a subsequent flush retries delivery without re-finalizing.
int audio_studio_vad_flush(
    AudioStudioVadHandle* handle, AudioStudioSpeechRegion* output,
    size_t capacity, size_t* written) {
    clear_error();
    if (!written) {
        set_error("The written-count pointer is required.");
        return 0;
    }
    *written = 0;
    if (!handle || !handle->session) {
        set_error("Invalid VAD handle.");
        if (written) *written = 0;
        return 0;
    }
    try {
        if (handle->push_retry_pending) {
            set_error("A push delivery retry is pending; retry push_audio before flushing.");
            return 0;
        }
        if (!handle->flushed) {
            std::vector<audio_studio::SpeechRegion> regions;
            if (!handle->session->flush(regions)) {
                set_error("VAD final flush failed; reset the session before retrying.");
                if (written) *written = 0;
                return 0;
            }
            handle->pending_regions.insert(handle->pending_regions.end(), regions.begin(), regions.end());
            handle->flushed = true;
        }
        if (!copy_regions(handle->pending_regions, output, capacity, written)) return 0;
        handle->pending_regions.clear();
        return 1;
    } catch (const std::exception& e) {
        set_error(e.what());
    } catch (...) {
        set_error("Unexpected error while flushing VAD audio.");
    }
    if (written) *written = 0;
    return 0;
}

// Starts a fresh stream and discards queued, not-yet-delivered regions.
int audio_studio_vad_reset(AudioStudioVadHandle* handle) {
    clear_error();
    if (!handle || !handle->session) {
        set_error("Invalid VAD handle.");
        return 0;
    }
    handle->session->reset();
    handle->pending_regions.clear();
    handle->flushed = false;
    handle->push_retry_pending = false;
    return 1;
}

// Error text is thread-local and remains valid until the next VAD API call on
// the same thread. Copy it if it must outlive that call.
const char* audio_studio_vad_last_error(void) {
    return vad_error.c_str();
}

}
