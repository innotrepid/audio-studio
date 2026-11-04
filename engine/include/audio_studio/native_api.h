#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#define AUDIO_STUDIO_API __declspec(dllexport)
#else
#define AUDIO_STUDIO_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle for an independent streaming Silero VAD session.
// Region timestamps (start_ms / end_ms) are milliseconds on the internal 16 kHz
// mono timeline after capture resampling. They match source duration but are not
// original device frame indices.
// Create requires a packaged silero_vad.onnx path and ONNX Runtime on the target.
typedef struct AudioStudioVadHandle AudioStudioVadHandle;
typedef struct AudioStudioSpeechRegion {
    uint64_t start_ms;
    uint64_t end_ms;
} AudioStudioSpeechRegion;

// Windows-oriented create: model_path is a NUL-terminated UTF-16 path.
AUDIO_STUDIO_API AudioStudioVadHandle* audio_studio_vad_create(
    const wchar_t* model_path, uint32_t sample_rate, uint32_t channels,
    float speech_threshold, uint64_t minimum_speech_ms,
    uint64_t minimum_silence_ms, uint64_t padding_ms);
// Cross-platform create: model_path_utf8 is a filesystem path to silero_vad.onnx.
// Preferred on Android/Linux; also valid on Windows when the path is UTF-8.
AUDIO_STUDIO_API AudioStudioVadHandle* audio_studio_vad_create_utf8(
    const char* model_path_utf8, uint32_t sample_rate, uint32_t channels,
    float speech_threshold, uint64_t minimum_speech_ms,
    uint64_t minimum_silence_ms, uint64_t padding_ms);
AUDIO_STUDIO_API void audio_studio_vad_destroy(AudioStudioVadHandle* handle);
// Pushes normalized float32 interleaved PCM; frame_count counts frames, not samples.
// Returns 1 on success, 0 on failure. On output-capacity failure, *written reports
// the required region count and no output regions are copied. The regions remain
// queued. Retry with a sufficiently large output buffer and do not replay the same
// PCM chunk: a pending retry only delivers retained regions and does not process
// the PCM arguments. Retry a pending push before calling flush. After successful
// flush, reset before pushing more audio. Calls on one handle must be serialized.
AUDIO_STUDIO_API int audio_studio_vad_push_audio(
    AudioStudioVadHandle* handle, const float* interleaved, size_t frame_count,
    AudioStudioSpeechRegion* output, size_t capacity, size_t* written);
// Finalizes the stream once. On output-capacity failure, *written reports the
// required region count and final regions remain queued. Retry flush with a larger
// buffer; the session is not finalized a second time. Flush is rejected while a
// push-delivery retry is pending; complete that retry first. After successful
// flush, reset before pushing more audio.
AUDIO_STUDIO_API int audio_studio_vad_flush(
    AudioStudioVadHandle* handle, AudioStudioSpeechRegion* output,
    size_t capacity, size_t* written);
// Discards queued, not-yet-delivered regions and starts a fresh stream, including
// clearing any pending push/flush capacity retry.
AUDIO_STUDIO_API int audio_studio_vad_reset(AudioStudioVadHandle* handle);
AUDIO_STUDIO_API const char* audio_studio_vad_last_error(void);

// Stable C ABI for thin native UI bindings. The returned string has static
// storage duration and must not be freed by the caller.
AUDIO_STUDIO_API const char* audio_studio_engine_status(void);
AUDIO_STUDIO_API int audio_studio_capture_device_count(void);
AUDIO_STUDIO_API const wchar_t* audio_studio_capture_device_name(int index);
AUDIO_STUDIO_API const wchar_t* audio_studio_capture_device_id(int index);
AUDIO_STUDIO_API int audio_studio_capture_device_select(const wchar_t* device_id);
AUDIO_STUDIO_API int audio_studio_recording_start(const wchar_t* output_path);
AUDIO_STUDIO_API int audio_studio_recording_stop(void);
AUDIO_STUDIO_API int audio_studio_recording_pause(void);
AUDIO_STUDIO_API int audio_studio_recording_resume(void);
AUDIO_STUDIO_API int audio_studio_recording_active(void);
AUDIO_STUDIO_API const wchar_t* audio_studio_recording_status(void);
AUDIO_STUDIO_API float audio_studio_recording_peak(void);
AUDIO_STUDIO_API int audio_studio_recording_clipping(void);
AUDIO_STUDIO_API const wchar_t* audio_studio_recover_partial_wav(const wchar_t* partial_path, const wchar_t* output_path);
AUDIO_STUDIO_API int audio_studio_processing_start(const wchar_t* input_path,
    const wchar_t* output_path, float threshold_dbfs, float attenuation_db,
    float gain_db, float high_pass_hz, int processing_engine, float denoiser_strength,
    const wchar_t* model_directory);
AUDIO_STUDIO_API void audio_studio_processing_cancel(void);
AUDIO_STUDIO_API int audio_studio_processing_active(void);
AUDIO_STUDIO_API float audio_studio_processing_progress(void);
AUDIO_STUDIO_API const wchar_t* audio_studio_processing_status(void);
AUDIO_STUDIO_API int audio_studio_analyze_wav_waveform(const wchar_t* input_path, unsigned int bucket_count, float* interleaved_peaks, unsigned long long* total_frames, unsigned int* sample_rate, unsigned short* channels);
AUDIO_STUDIO_API int audio_studio_trim_wav(const wchar_t* input_path, const wchar_t* output_path, unsigned long long start_frame, unsigned long long end_frame);

#ifdef __cplusplus
}
#endif
