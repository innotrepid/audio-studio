#pragma once

#if defined(_WIN32)
#define AUDIO_STUDIO_API __declspec(dllexport)
#else
#define AUDIO_STUDIO_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Stable C ABI for thin native UI bindings. The returned string has static
// storage duration and must not be freed by the caller.
AUDIO_STUDIO_API const char* audio_studio_engine_status(void);
AUDIO_STUDIO_API int audio_studio_capture_device_count(void);
AUDIO_STUDIO_API const wchar_t* audio_studio_capture_device_name(int index);
AUDIO_STUDIO_API const wchar_t* audio_studio_capture_device_id(int index);
AUDIO_STUDIO_API int audio_studio_capture_device_select(const wchar_t* device_id);
AUDIO_STUDIO_API int audio_studio_recording_start(const wchar_t* output_path);
AUDIO_STUDIO_API int audio_studio_recording_stop(void);
AUDIO_STUDIO_API int audio_studio_recording_active(void);
AUDIO_STUDIO_API const wchar_t* audio_studio_recording_status(void);
AUDIO_STUDIO_API float audio_studio_recording_peak(void);
AUDIO_STUDIO_API int audio_studio_recording_clipping(void);
AUDIO_STUDIO_API int audio_studio_processing_start(const wchar_t* input_path,
    const wchar_t* output_path, float threshold_dbfs, float attenuation_db);
AUDIO_STUDIO_API void audio_studio_processing_cancel(void);
AUDIO_STUDIO_API int audio_studio_processing_active(void);
AUDIO_STUDIO_API float audio_studio_processing_progress(void);
AUDIO_STUDIO_API const wchar_t* audio_studio_processing_status(void);

#ifdef __cplusplus
}
#endif
