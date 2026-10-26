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

#ifdef __cplusplus
}
#endif
