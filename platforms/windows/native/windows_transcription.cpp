#include "audio_studio/native_api.h"
#include "audio_studio/transcription.hpp"
#include "audio_studio/wav_audio_reader.hpp"

#include <windows.h>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace {
std::mutex g_error_mutex;
std::string g_last_error;

void set_error(const std::string& message) {
    std::lock_guard<std::mutex> lock(g_error_mutex);
    g_last_error = message;
}

std::string wide_to_utf8(const wchar_t* wide) {
    if (wide == nullptr || wide[0] == L'\0') return {};
    const int needed = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1) return {};
    std::string out(static_cast<std::size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), needed, nullptr, nullptr);
    return out;
}
}  // namespace

extern "C" AUDIO_STUDIO_API const char* audio_studio_transcription_last_error(void) {
    std::lock_guard<std::mutex> lock(g_error_mutex);
    return g_last_error.c_str();
}

extern "C" AUDIO_STUDIO_API int audio_studio_transcribe_wav(
    const wchar_t* model_path, const wchar_t* wav_path,
    const char* language_utf8, int translate_to_english,
    char* out_utf8, size_t out_capacity) {
    if (out_utf8 == nullptr || out_capacity == 0) {
        set_error("Transcription output buffer is required.");
        return 0;
    }
    out_utf8[0] = '\0';
    const std::string model = wide_to_utf8(model_path);
    const std::string wav = wide_to_utf8(wav_path);
    if (model.empty() || wav.empty()) {
        set_error("Model path and WAV path are required.");
        return 0;
    }

    std::vector<float> mono_16k;
    std::string decode_error;
    if (!audio_studio::load_wav_for_transcription(wav, mono_16k, &decode_error) ||
        mono_16k.empty()) {
        set_error(decode_error.empty() ? "Could not decode WAV for transcription." : decode_error);
        return 0;
    }

    audio_studio::TranscriptionRequest request;
    request.model_path = model;
    request.mono_pcm_16khz = std::move(mono_16k);
    request.language = language_utf8 == nullptr || language_utf8[0] == '\0' ? "auto" : language_utf8;
    request.translate_to_english = translate_to_english != 0;

    const auto result = audio_studio::transcribe(request);
    if (!result) {
        set_error(result.error_message.empty() ? "Transcription failed." : result.error_message);
        return 0;
    }

    if (result.text.size() + 1 > out_capacity) {
        set_error("Transcription text exceeds the provided output buffer.");
        return 0;
    }
    std::memcpy(out_utf8, result.text.c_str(), result.text.size() + 1);
    set_error({});
    return 1;
}
