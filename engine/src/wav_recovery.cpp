#include "audio_studio/wav_recovery.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <limits>
#include <system_error>

namespace audio_studio {
namespace {

std::uint16_t read_u16(const unsigned char* bytes) {
    return static_cast<std::uint16_t>(bytes[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[1]) << 8U);
}

std::uint32_t read_u32(const unsigned char* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

void write_u32(unsigned char* bytes, std::uint32_t value) {
    bytes[0] = static_cast<unsigned char>(value & 0xffU);
    bytes[1] = static_cast<unsigned char>((value >> 8U) & 0xffU);
    bytes[2] = static_cast<unsigned char>((value >> 16U) & 0xffU);
    bytes[3] = static_cast<unsigned char>((value >> 24U) & 0xffU);
}

bool has_owned_partial_suffix(const std::string& filename, std::size_t& suffix_start) {
    suffix_start = filename.rfind(".partial-");
    if (suffix_start == std::string::npos) return false;
    const auto digits_start = suffix_start + 9U;
    if (digits_start >= filename.size()) return false;
    for (std::size_t i = digits_start; i < filename.size(); ++i) {
        if (filename[i] < '0' || filename[i] > '9') return false;
    }
    return true;
}

}  // namespace

bool recover_partial_wav(const std::filesystem::path& partial_path,
                         const std::filesystem::path& output_path,
                         std::string* error) {
    const auto fail = [error](const char* message) {
        if (error != nullptr) *error = message;
        return false;
    };

    const auto partial_name = partial_path.filename().string();
    std::size_t suffix_start = 0U;
    if (!has_owned_partial_suffix(partial_name, suffix_start)) {
        return fail("The source is not an Audio Studio temporary WAV file.");
    }
    if (partial_path.parent_path() != output_path.parent_path() ||
        output_path.filename().string() != partial_name.substr(0U, suffix_start)) {
        return fail("The recovery destination does not match the original recording path.");
    }

    // Do not follow symbolic links during recovery: repairing a header through a
    // link would mutate a different file that merely happens to be targeted by
    // a writer-owned-looking temporary filename.
    std::error_code path_error;
    const auto source_status = std::filesystem::symlink_status(partial_path, path_error);
    if (path_error || !std::filesystem::is_regular_file(source_status)) {
        return fail("The recovery source must be a regular file, not a link or special file.");
    }

    if (std::filesystem::exists(output_path, path_error)) {
        return fail("The recovery destination already exists; it was not changed.");
    }
    if (path_error) return fail("Could not inspect the recovery destination.");

    const auto file_size = std::filesystem::file_size(partial_path, path_error);
    if (path_error || file_size < 44U) {
        return fail("The temporary WAV is missing or too short to recover.");
    }
    const auto data_size = file_size - 44U;
    constexpr std::uint64_t max_data_size =
        static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) - 36U;
    if (data_size == 0U || data_size > max_data_size) {
        return fail("The temporary WAV payload has an unsupported size.");
    }

    std::fstream stream(partial_path, std::ios::binary | std::ios::in | std::ios::out);
    if (!stream.is_open()) return fail("Could not open the temporary WAV for recovery.");

    std::array<unsigned char, 44U> header{};
    stream.read(reinterpret_cast<char*>(header.data()),
                static_cast<std::streamsize>(header.size()));
    if (!stream || header[0] != 'R' || header[1] != 'I' || header[2] != 'F' ||
        header[3] != 'F' || header[8] != 'W' || header[9] != 'A' ||
        header[10] != 'V' || header[11] != 'E' || header[12] != 'f' ||
        header[13] != 'm' || header[14] != 't' || header[15] != ' ' ||
        read_u32(header.data() + 16U) != 16U ||
        read_u16(header.data() + 20U) != 1U ||
        header[36] != 'd' || header[37] != 'a' ||
        header[38] != 't' || header[39] != 'a') {
        return fail("The temporary file does not contain a supported PCM WAV header.");
    }

    const auto channels = read_u16(header.data() + 22U);
    const auto sample_rate = read_u32(header.data() + 24U);
    const auto byte_rate = read_u32(header.data() + 28U);
    const auto block_align = read_u16(header.data() + 32U);
    const auto bits_per_sample = read_u16(header.data() + 34U);
    if ((channels != 1U && channels != 2U) || sample_rate < 8000U ||
        sample_rate > 192000U || byte_rate != sample_rate * channels * 2U ||
        block_align != channels * 2U || bits_per_sample != 16U ||
        data_size % block_align != 0U) {
        return fail("The temporary WAV format or final audio frame is invalid.");
    }

    // The writer leaves zero lengths in the temporary header. Rebuild lengths
    // from the actual payload only after validating the entire file structure.
    write_u32(header.data() + 4U, static_cast<std::uint32_t>(36U + data_size));
    write_u32(header.data() + 40U, static_cast<std::uint32_t>(data_size));
    stream.seekp(0, std::ios::beg);
    stream.write(reinterpret_cast<const char*>(header.data()),
                 static_cast<std::streamsize>(header.size()));
    stream.flush();
    if (!stream) return fail("Could not repair the temporary WAV header.");
    stream.close();
    if (stream.fail()) return fail("Could not close the recovered WAV safely.");

    // A plain rename may replace a destination created concurrently on POSIX.
    // Creating a hard link is an atomic no-clobber publication: it fails if the
    // destination already exists. Both paths are required to share a directory.
    path_error.clear();
    std::filesystem::create_hard_link(partial_path, output_path, path_error);
    if (path_error) {
        return fail("Could not publish the recovered WAV without replacing an existing file; the temporary file was retained.");
    }

    // Publication is already complete and the recovered output is valid. If
    // cleanup fails, retain the extra partial name rather than risk losing audio.
    path_error.clear();
    std::filesystem::remove(partial_path, path_error);
    if (error != nullptr) error->clear();
    return true;
}

}  // namespace audio_studio
