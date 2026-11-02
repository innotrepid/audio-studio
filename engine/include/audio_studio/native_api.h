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
typedef struct AudioStudioVadHandle AudioStudioVadHandle;
typedef struct AudioStudioSpeechRegion {
    uint64_t start_ms;
    uint64_t end_ms;
} AudioStudioSpeechRegion;

AUDIO_STUDIO_API AudioStudioVadHandle* audio_studio_vad_create(
    const wchar_t* model_path, uint32_t sample_rate, uint32_t channels,
    float speech_threshold, uint64_t minimum_speech_ms,
    uint64_t minimum_silence_ms, uint64_t padding_ms);
AUDIO_STUDIO_API void audio_studio_vad_destroy(AudioStudioVadHandle* handle);
AUDIO_STUDIO_API int audio_studio_vad_push_audio(
    AudioStudioVadHandle* handle, const float* interleaved, size_t frame_count,
    AudioStudioSpeechRegion* output, size_t capacity, size_t* written);
AUDIO_STUDIO_API int audio_studio_vad_flush(
    AudioStudioVadHandle* handle, AudioStudioSpeechRegion* output,
    size_t capacity, size_t* written);
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
