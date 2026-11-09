#include "audio_studio/transcription_json.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
}

int main() {
    using namespace audio_studio;

    TranscriptionResult success;
    success.detected_language = "en";
    success.text = "Recognized: \"hello\"\\take\nnext\tline";
    success.segments.push_back({120U, 980U, " \"hello\"\\take\n"});
    const std::string expected =
        "{\"text\":\"Recognized: \\\"hello\\\"\\\\take\\nnext\\tline\","
        "\"detectedLanguage\":\"en\",\"segments\":["
        "{\"startMs\":120,\"endMs\":980,\"text\":\" \\\"hello\\\"\\\\take\\n\"}]}";
    const auto actual = serialize_transcription_result_json(success);
    require(actual == expected, "Successful transcription JSON did not preserve and escape recognized text.");

    TranscriptionResult failure;
    failure.error = TranscriptionError::ModelLoadFailed;
    failure.error_message = "Model \"tiny\" not found";
    require(serialize_transcription_result_json(failure) ==
                "{\"error\":\"Model \\\"tiny\\\" not found\"}",
            "Error transcription JSON is malformed.");

    TranscriptionResult empty;
    require(serialize_transcription_result_json(empty) ==
                "{\"text\":\"\",\"detectedLanguage\":\"\",\"segments\":[]}",
            "Empty successful result must still serialize as valid JSON for UI validation.");

    std::cout << "Transcription JSON serialization regression tests passed.\n";
    return 0;
}
