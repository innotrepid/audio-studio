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

AudioStudioVadHandle* create_session_from_path(
    const std::string& path, uint32_t sample_rate, uint32_t channels,
    float speech_threshold, uint64_t minimum_speech_ms,
    uint64_t minimum_silence_ms, uint64_t padding_ms) {
    if (path.empty() || !std::isfinite(speech_threshold) ||
        speech_threshold < 0.0F || speech_threshold > 1.0F) {
        set_error("Invalid model path or speech threshold.");
        return nullptr;
    }
    audio_studio::VadOptions options;
    options.speech_threshold = speech_threshold;
    options.minimum_speech_ms = minimum_speech_ms;
    options.minimum_silence_ms = minimum_silence_ms;
    options.padding_ms = padding_ms;
    std::string error;
#ifdef AUDIO_STUDIO_NATIVE_VAD_TESTING
    auto session = audio_studio::native_vad_test::create_session(
        path, sample_rate, channels, options, error);
#else
    auto session = audio_studio::NativeVadSession::create(
        path, sample_rate, channels, options, error);
#endif
    if (!session) {
        set_error(error.empty() ? "Could not create VAD session." : error.c_str());
        return nullptr;
    }
    auto* handle = new AudioStudioVadHandle;
    handle->session = std::move(session);
    return handle;
}
}  // namespace

extern "C" {

AudioStudioVadHandle* audio_studio_vad_create(
    const wchar_t* model_path, uint32_t sample_rate, uint32_t channels,
    float speech_threshold, uint64_t minimum_speech_ms,
    uint64_t minimum_silence_ms, uint64_t padding_ms) {
    clear_error();
    if (!model_path || !*model_path) {
        set_error("Invalid model path or speech threshold.");
        return nullptr;
    }
    try {
#if defined(AUDIO_STUDIO_NATIVE_VAD_TESTING)
        return create_session_from_path(
            "test-session", sample_rate, channels, speech_threshold,
            minimum_speech_ms, minimum_silence_ms, padding_ms);
#elif defined(_WIN32)
        const std::string path = std::filesystem::path(model_path).u8string();
        return create_session_from_path(
            path, sample_rate, channels, speech_threshold, minimum_speech_ms,
            minimum_silence_ms, padding_ms);
#else
        set_error("audio_studio_vad_create requires a UTF-16 path on Windows; "
                  "use audio_studio_vad_create_utf8 on this platform.");
        return nullptr;
#endif
    } catch (const std::exception& e) {
        set_error(e.what());
    } catch (...) {
        set_error("Could not allocate native VAD handle.");
    }
    return nullptr;
}

AudioStudioVadHandle* audio_studio_vad_create_utf8(
    const char* model_path_utf8, uint32_t sample_rate, uint32_t channels,
    float speech_threshold, uint64_t minimum_speech_ms,
    uint64_t minimum_silence_ms, uint64_t padding_ms) {
    clear_error();
    if (!model_path_utf8 || !*model_path_utf8) {
        set_error("Invalid model path or speech threshold.");
        return nullptr;
    }
    try {
        return create_session_from_path(
            std::string(model_path_utf8), sample_rate, channels, speech_threshold,
            minimum_speech_ms, minimum_silence_ms, padding_ms);
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
            return 0;
        }
        handle->pending_regions.insert(
            handle->pending_regions.end(), regions.begin(), regions.end());
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
                return 0;
            }
            handle->pending_regions.insert(
                handle->pending_regions.end(), regions.begin(), regions.end());
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

const char* audio_studio_vad_last_error(void) {
    return vad_error.c_str();
}

}
