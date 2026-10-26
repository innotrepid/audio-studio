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
AUDIO_STUDIO_API int audio_studio_recording_start(const wchar_t* output_path);
AUDIO_STUDIO_API int audio_studio_recording_stop(void);
AUDIO_STUDIO_API int audio_studio_recording_active(void);
AUDIO_STUDIO_API const wchar_t* audio_studio_recording_status(void);

#ifdef __cplusplus
}
#endif
