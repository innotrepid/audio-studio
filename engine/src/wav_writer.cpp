#include "audio_studio/wav_writer.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <system_error>
#include <cmath>
#include <limits>

namespace audio_studio {
namespace {

constexpr std::uint64_t kMaxDataBytes =
    static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) - 36U;
std::atomic<std::uint64_t> g_temporary_file_counter{0U};

void write_u16_le(std::ofstream& stream, std::uint16_t value) {
    const char bytes[2] = {static_cast<char>(value & 0xffU),
                           static_cast<char>((value >> 8U) & 0xffU)};
    stream.write(bytes, 2);
}

void write_u32_le(std::ofstream& stream, std::uint32_t value) {
    const char bytes[4] = {static_cast<char>(value & 0xffU),
                           static_cast<char>((value >> 8U) & 0xffU),
                           static_cast<char>((value >> 16U) & 0xffU),
                           static_cast<char>((value >> 24U) & 0xffU)};
    stream.write(bytes, 4);
}

void write_header(std::ofstream& stream, AudioFormat format, std::uint32_t data_bytes) {
    stream.seekp(0, std::ios::beg);
    stream.write("RIFF", 4);
    write_u32_le(stream, 36U + data_bytes);
    stream.write("WAVE", 4);
    stream.write("fmt ", 4);
    write_u32_le(stream, 16U);
    write_u16_le(stream, 1U);
    write_u16_le(stream, format.channel_count);
    write_u32_le(stream, format.sample_rate_hz);
    const auto byte_rate = format.sample_rate_hz * format.channel_count * 2U;
    write_u32_le(stream, byte_rate);
    write_u16_le(stream, static_cast<std::uint16_t>(format.channel_count * 2U));
    write_u16_le(stream, 16U);
    stream.write("data", 4);
    write_u32_le(stream, data_bytes);
}

}  // namespace

WavWriter::~WavWriter() {
    // An unfinalized recording is incomplete; never turn it into a seemingly valid WAV.
    if (!finalized_) abort();
}

bool WavWriter::open(const std::string& path, AudioFormat format) {
    return open_path(std::filesystem::path(path), format);
}

bool WavWriter::open_path(const std::filesystem::path& path, AudioFormat format) {
    if (stream_.is_open()) {
        set_error("A WAV file is already open.");
        return false;
    }
    if (!format.is_valid()) {
        set_error("Unsupported audio format.");
        return false;
    }

    std::error_code path_error;
    if (std::filesystem::exists(path, path_error)) {
        set_error("The output file already exists; refusing to overwrite it.");
        return false;
    }
    if (path_error) {
        set_error("Could not inspect the output destination.");
        return false;
    }

    // Keep the temporary beside the destination so final rename stays on the
    // same filesystem and the final filename is invisible until the header is valid.
    for (int attempt = 0; attempt < 32; ++attempt) {
        auto candidate = path;
        candidate += ".partial-" +
            std::to_string(g_temporary_file_counter.fetch_add(1U, std::memory_order_relaxed));
        path_error.clear();
        if (std::filesystem::exists(candidate, path_error)) continue;
        if (path_error) {
            set_error("Could not inspect a temporary output path.");
            return false;
        }
        // Create first, then reopen for read/write header updates. Some
        // standard libraries require the file to exist for in|out mode.
        {
            std::ofstream create(candidate, std::ios::binary | std::ios::out);
            if (!create.is_open()) continue;
        }
        stream_.clear();
        stream_.open(candidate, std::ios::binary | std::ios::in | std::ios::out);
        if (stream_.is_open()) {
            temporary_path_ = candidate;
            output_path_ = path;
            break;
        }
        std::filesystem::remove(candidate, path_error);
    }
    if (!stream_.is_open()) {
        set_error("Could not create a temporary WAV output file.");
        return false;
    }
    format_ = format;
    frames_written_ = 0U;
    finalized_ = false;
    last_error_.clear();
    write_header(stream_, format_, 0U);
    if (!stream_) {
        set_error("Could not write the WAV header.");
        abort();
        return false;
    }
    return true;
}

bool WavWriter::write_frames(const float* samples, std::size_t frame_count) {
    if (!stream_.is_open() || finalized_) {
        set_error("WAV writer is not open.");
        return false;
    }
    if (frame_count == 0U) {
        return true;
    }
    if (samples == nullptr) {
        set_error("Audio sample pointer is null.");
        return false;
    }

    const std::uint64_t sample_count =
        static_cast<std::uint64_t>(frame_count) * format_.channel_count;
    const std::uint64_t bytes_to_write = sample_count * 2U;
    if (sample_count / format_.channel_count != frame_count ||
        frames_written_ > kMaxDataBytes / (format_.channel_count * 2U) ||
        bytes_to_write > kMaxDataBytes -
            frames_written_ * format_.channel_count * 2U) {
        set_error("Recording exceeds the WAV format size limit.");
        return false;
    }

    for (std::uint64_t i = 0; i < sample_count; ++i) {
        const float sample = samples[i];
        if (!std::isfinite(sample)) {
            set_error("Audio buffer contains a non-finite sample.");
            return false;
        }
    }

    // Batch PCM conversion and file writes. A separate stream.write() for every
    // sample made long recordings needlessly expensive (millions of tiny writes).
    // The fixed-size buffer bounds memory use regardless of recording duration.
    constexpr std::size_t kSamplesPerBatch = 4096U;
    std::array<char, kSamplesPerBatch * 2U> encoded{};
    std::uint64_t offset = 0U;
    while (offset < sample_count) {
        const auto count = static_cast<std::size_t>(
            std::min<std::uint64_t>(kSamplesPerBatch, sample_count - offset));
        for (std::size_t i = 0U; i < count; ++i) {
            const float clipped = std::clamp(samples[offset + i], -1.0F, 1.0F);
            const auto pcm = static_cast<std::int16_t>(
                clipped <= -1.0F ? -32768 : std::lround(clipped * 32767.0F));
            const auto bits = static_cast<std::uint16_t>(pcm);
            encoded[i * 2U] = static_cast<char>(bits & 0xffU);
            encoded[i * 2U + 1U] = static_cast<char>((bits >> 8U) & 0xffU);
        }
        stream_.write(encoded.data(), static_cast<std::streamsize>(count * 2U));
        if (!stream_) {
            set_error("Failed while writing audio samples.");
            return false;
        }
        offset += count;
    }
    frames_written_ += frame_count;
    return true;
}

bool WavWriter::finalize() {
    if (!stream_.is_open()) {
        set_error("WAV writer is not open.");
        return false;
    }
    const auto data_bytes = static_cast<std::uint32_t>(
        frames_written_ * format_.channel_count * 2U);
    write_header(stream_, format_, data_bytes);
    stream_.flush();
    const bool flushed = static_cast<bool>(stream_);
    stream_.close();
    if (!flushed) {
        set_error("Could not finalize the WAV header or flush the temporary file.");
        abort();
        return false;
    }

    // Publish without replacement. A prior exists()+rename() sequence has a
    // time-of-check/time-of-use race on platforms where rename replaces an
    // existing destination. A same-directory hard-link creation is atomic and
    // fails if another process created the destination after open().
    std::error_code path_error;
    std::filesystem::create_hard_link(temporary_path_, output_path_, path_error);
    if (path_error) {
        set_error("Could not publish the finalized WAV without replacing an existing file.");
        abort();
        return false;
    }

    // The final path now references the fully flushed file. Remove the
    // temporary name; if cleanup fails, keep the valid final output and report
    // success rather than deleting or invalidating the published recording.
    std::error_code cleanup_error;
    std::filesystem::remove(temporary_path_, cleanup_error);
    if (!cleanup_error) temporary_path_.clear();
    output_path_.clear();
    finalized_ = true;
    return true;
}

void WavWriter::preserve_partial() noexcept {
    if (stream_.is_open()) {
        stream_.flush();
        stream_.close();
    }
    // Do not publish an incomplete final WAV. Leave the writer-owned .partial-N
    // file in place so the recovery flow can validate and repair its header.
    output_path_.clear();
    finalized_ = true;
}

void WavWriter::abort() noexcept {
    if (stream_.is_open()) {
        stream_.close();
    }
    if (!temporary_path_.empty()) {
        std::error_code ignored;
        std::filesystem::remove(temporary_path_, ignored);
        temporary_path_.clear();
    }
    output_path_.clear();
    finalized_ = true;
}

void WavWriter::set_error(const char* message) {
    last_error_ = message;
}

}  // namespace audio_studio
