#include "audio_studio/native_api.h"
#include "audio_studio/audio_types.hpp"

extern "C" AUDIO_STUDIO_API const char* audio_studio_engine_status(void) {
    static const audio_studio::AudioFormat baseline{48000U, 1U};
    return baseline.is_valid()
        ? "Shared C++ audio core connected"
        : "Shared C++ audio core validation failed";
}
