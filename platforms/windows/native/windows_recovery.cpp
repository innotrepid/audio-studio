#include "audio_studio/native_api.h"
#include "audio_studio/wav_recovery.hpp"

#include <filesystem>
#include <memory>
#include <string>

extern "C" AUDIO_STUDIO_API const wchar_t* audio_studio_recover_partial_wav(
    const wchar_t* partial_path, const wchar_t* output_path) {
    thread_local std::wstring result;
    if (partial_path == nullptr || output_path == nullptr) {
        result = L"Recovery requires both a temporary partial path and a destination path.";
        return result.c_str();
    }
    auto message = std::make_unique<std::string>();
    if (!audio_studio::recover_partial_wav(
            std::filesystem::path(partial_path),
            std::filesystem::path(output_path),
            message.get())) {
        result.assign(message->begin(), message->end());
        return result.c_str();
    }
    result = L"Recovered a complete PCM WAV from the interrupted recording.";
    return result.c_str();
}
