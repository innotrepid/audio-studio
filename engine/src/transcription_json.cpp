#include "audio_studio/transcription_json.hpp"

namespace audio_studio {
namespace {

std::string escape_json_string(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 8U);
    constexpr char hex[] = "0123456789abcdef";
    for (const unsigned char ch : value) {
        switch (ch) {
        case '"': escaped += "\\\""; break;
        case '\\': escaped += "\\\\"; break;
        case '\b': escaped += "\\b"; break;
        case '\f': escaped += "\\f"; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default:
            if (ch < 0x20U) {
                escaped += "\\u00";
                escaped += hex[(ch >> 4U) & 0x0fU];
                escaped += hex[ch & 0x0fU];
            } else {
                escaped += static_cast<char>(ch);
            }
        }
    }
    return escaped;
}

}  // namespace

std::string serialize_transcription_result_json(const TranscriptionResult& result) {
    if (!result) {
        return std::string("{\"error\":\"") + escape_json_string(result.error_message) + "\"}";
    }

    std::string payload = std::string("{\"text\":\"") + escape_json_string(result.text) +
        "\",\"detectedLanguage\":\"" + escape_json_string(result.detected_language) +
        "\",\"segments\":[";
    for (std::size_t index = 0; index < result.segments.size(); ++index) {
        if (index != 0U) payload += ",";
        const auto& segment = result.segments[index];
        payload += "{\"startMs\":" + std::to_string(segment.start_ms) +
            ",\"endMs\":" + std::to_string(segment.end_ms) +
            ",\"text\":\"" + escape_json_string(segment.text) + "\"}";
    }
    payload += "]}";
    return payload;
}

}  // namespace audio_studio
