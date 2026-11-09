#include "audio_studio/transcription.hpp"
#include "audio_studio/wav_audio_reader.hpp"

#include <cctype>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    using namespace audio_studio;
    if (argc != 3) {
        std::cerr << "Expected paths to the bundled Whisper model and recorded-speech WAV fixture.\n";
        return 1;
    }
    std::vector<float> pcm;
    std::string error;
    if (!load_wav_for_transcription(argv[2], pcm, &error) || pcm.empty()) {
        std::cerr << "Could not decode speech fixture for Whisper: " << error << "\n";
        return 2;
    }
    TranscriptionRequest request;
    request.model_path = argv[1];
    request.mono_pcm_16khz = std::move(pcm);
    request.language = "auto";
    const auto result = transcribe(request);
    if (!result) {
        std::cerr << "Whisper integration inference failed: " << result.error_message << "\n";
        return 3;
    }
    if (result.text.empty() || result.segments.empty()) {
        std::cerr << "Whisper returned no recognized speech segments.\n";
        return 4;
    }
    bool has_word_character = false;
    for (const unsigned char ch : result.text) {
        if (std::isalnum(ch) != 0 || ch >= 0x80U) {
            has_word_character = true;
            break;
        }
    }
    if (!has_word_character) {
        std::cerr << "Whisper output contained no recognizable word characters.\n";
        return 5;
    }
    std::uint64_t previous_start = 0U;
    for (const auto& segment : result.segments) {
        if (segment.start_ms >= segment.end_ms || segment.start_ms < previous_start) {
            std::cerr << "Whisper returned invalid or out-of-order segment timestamps.\n";
            return 6;
        }
        previous_start = segment.start_ms;
    }
    std::cout << "Whisper speech integration passed: language="
              << result.detected_language << ", segments=" << result.segments.size()
              << ", characters=" << result.text.size() << "\n";
    return 0;
}
