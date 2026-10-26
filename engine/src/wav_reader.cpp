#include "audio_studio/wav_reader.hpp"

#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace audio_studio {
namespace {

std::uint16_t read_u16_le(const unsigned char* bytes) noexcept {
    return static_cast<std::uint16_t>(bytes[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[1]) << 8U);
}

std::uint32_t read_u32_le(const unsigned char* bytes) noexcept {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

bool tag_is(const std::vector<unsigned char>& bytes, std::size_t offset,
            const char* tag) noexcept {
    return offset <= bytes.size() && bytes.size() - offset >= 4U &&
           bytes[offset] == static_cast<unsigned char>(tag[0]) &&
           bytes[offset + 1U] == static_cast<unsigned char>(tag[1]) &&
           bytes[offset + 2U] == static_cast<unsigned char>(tag[2]) &&
           bytes[offset + 3U] == static_cast<unsigned char>(tag[3]);
}

bool fail(std::string* error, const char* message) {
    if (error != nullptr) *error = message;
    return false;
}

}  // namespace

bool WavReader::read(const std::string& path, AudioBuffer& destination,
                     std::string* error) {
    if (error != nullptr) error->clear();

    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream.is_open()) return fail(error, "Could not open the WAV file.");

    const std::streamoff end = stream.tellg();
    if (end < 12) return fail(error, "WAV file is too small to contain a RIFF header.");
    const auto file_size = static_cast<std::uint64_t>(end);
    if (file_size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return fail(error, "WAV file is too large for this platform.");
    }

    std::vector<unsigned char> bytes(static_cast<std::size_t>(file_size));
    stream.seekg(0, std::ios::beg);
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream) return fail(error, "Could not read the complete WAV file.");

    if (!tag_is(bytes, 0U, "RIFF") || !tag_is(bytes, 8U, "WAVE")) {
        return fail(error, "File is not a RIFF/WAVE file.");
    }

    const std::uint64_t riff_end = static_cast<std::uint64_t>(read_u32_le(bytes.data() + 4U)) + 8U;
    if (riff_end < 12U || riff_end > file_size) {
        return fail(error, "RIFF size is invalid or the file is truncated.");
    }

    bool have_format = false;
    bool have_data = false;
    std::uint16_t encoding = 0U;
    std::uint16_t channels = 0U;
    std::uint16_t bits_per_sample = 0U;
    std::uint16_t block_align = 0U;
    std::uint32_t sample_rate = 0U;
    std::size_t data_offset = 0U;
    std::uint32_t data_size = 0U;

    std::uint64_t cursor = 12U;
    while (cursor + 8U <= riff_end) {
        const auto offset = static_cast<std::size_t>(cursor);
        const std::uint32_t chunk_size = read_u32_le(bytes.data() + offset + 4U);
        const std::uint64_t payload = cursor + 8U;
        const std::uint64_t payload_end = payload + chunk_size;
        const std::uint64_t padded_end = payload_end + (chunk_size & 1U);
        if (payload_end < payload || padded_end < payload_end || padded_end > riff_end) {
            return fail(error, "WAV contains a truncated or out-of-bounds chunk.");
        }

        if (tag_is(bytes, offset, "fmt ")) {
            if (have_format || chunk_size < 16U) {
                return fail(error, "WAV format chunk is duplicated or too short.");
            }
            const auto* fmt = bytes.data() + static_cast<std::size_t>(payload);
            encoding = read_u16_le(fmt);
            channels = read_u16_le(fmt + 2U);
            sample_rate = read_u32_le(fmt + 4U);
            const std::uint32_t byte_rate = read_u32_le(fmt + 8U);
            block_align = read_u16_le(fmt + 12U);
            bits_per_sample = read_u16_le(fmt + 14U);
            if (encoding != 1U || bits_per_sample != 16U ||
                (channels != 1U && channels != 2U) ||
                sample_rate < 8000U || sample_rate > 192000U ||
                block_align != channels * 2U ||
                byte_rate != sample_rate * static_cast<std::uint32_t>(block_align)) {
                return fail(error, "Only valid mono/stereo PCM16 WAV files are supported.");
            }
            have_format = true;
        } else if (tag_is(bytes, offset, "data")) {
            if (have_data) return fail(error, "WAV contains multiple data chunks.");
            data_offset = static_cast<std::size_t>(payload);
            data_size = chunk_size;
            have_data = true;
        }
        cursor = padded_end;
    }

    if (cursor != riff_end) return fail(error, "WAV contains an incomplete chunk header.");
    if (!have_format || !have_data) return fail(error, "WAV is missing its format or data chunk.");
    if (data_size == 0U || data_size % block_align != 0U) {
        return fail(error, "WAV audio data is empty or not frame-aligned.");
    }

    const std::size_t sample_count = static_cast<std::size_t>(data_size / 2U);
    AudioBuffer decoded;
    decoded.format = {sample_rate, channels};
    decoded.samples.reserve(sample_count);
    for (std::size_t i = 0; i < sample_count; ++i) {
        const auto raw = read_u16_le(bytes.data() + data_offset + i * 2U);
        const auto signed_sample = static_cast<std::int16_t>(raw);
        decoded.samples.push_back(signed_sample < 0
            ? static_cast<float>(signed_sample) / 32768.0F
            : static_cast<float>(signed_sample) / 32767.0F);
    }

    if (!decoded.is_valid()) return fail(error, "Decoded WAV audio failed validation.");
    destination = std::move(decoded);
    return true;
}

}  // namespace audio_studio
