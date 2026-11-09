#pragma once

#include "audio_studio/transcription.hpp"

#include <string>

namespace audio_studio {

// Serializes the native transcription result using strict JSON string escaping.
// The Android JNI bridge returns this payload to Kotlin's JSONObject parser.
std::string serialize_transcription_result_json(const TranscriptionResult& result);

}  // namespace audio_studio
