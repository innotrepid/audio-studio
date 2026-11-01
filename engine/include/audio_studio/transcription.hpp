#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace audio_studio {

// Transcription consumes normalized mono float PCM at 16 kHz. Platform adapters
// are responsible for decoding/resampling source media before calling the engine.
inline constexpr std::uint32_t kTranscriptionSampleRateHz = 16000U;
inline constexpr const char* kDefaultTranscriptionModel = "whisper-tiny-multilingual";

enum class TranscriptionError {
    None = 0,
    InvalidRequest,
    ModelNotFound,
    ModelLoadFailed,
    InferenceFailed,
    Cancelled
};

struct TranscriptionSegment {
    std::uint64_t start_ms{0U};
    std::uint64_t end_ms{0U};
    std::string text;
};

struct TranscriptionRequest {
    // Absolute filesystem path to the bundled ggml model. Android must first
    // copy the packaged asset to app-private storage; Windows uses package path.
    std::string model_path;
    std::vector<float> mono_pcm_16khz;
    // Empty or "auto" lets Whisper detect language; otherwise ISO language code.
    std::string language{"auto"};
    bool translate_to_english{false};
};

struct TranscriptionResult {
    TranscriptionError error{TranscriptionError::None};
    std::string error_message;
    std::string detected_language;
    std::string text;
    std::vector<TranscriptionSegment> segments;

    explicit operator bool() const noexcept {
        return error == TranscriptionError::None;
    }
};

// Loads the bundled model and performs local inference. No network access is
// permitted by this API. The implementation owns runtime/context resources.
TranscriptionResult transcribe(const TranscriptionRequest& request);

}  // namespace audio_studio
