#include "audio_studio/wav_writer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace audio_studio {
namespace {

constexpr std::uint64_t kMaxDataBytes =
    static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) - 36U;

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
    if (stream_.is_open() && !finalized_) {
        (void)finalize();
    }
}

bool WavWriter::open(const std::string& path, AudioFormat format) {
    return open(std::filesystem::path(path), format);
}

bool WavWriter::open(const std::filesystem::path& path, AudioFormat format) {
    if (stream_.is_open()) {
        set_error("A WAV file is already open.");
        return false;
    }
    if (!format.is_valid()) {
        set_error("Unsupported audio format.");
        return false;
    }

    stream_.clear();
    stream_.open(path, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
    if (!stream_.is_open()) {
        // Some standard-library implementations require an existing file for in|out.
        stream_.clear();
        stream_.open(path, std::ios::binary | std::ios::out | std::ios::trunc);
        if (!stream_.is_open()) {
            set_error("Could not open the output file.");
            return false;
        }
        // Output-only streams cannot seek reliably on every implementation, so fail
        // early instead of risking a corrupt final header.
        stream_.close();
        set_error("Output file does not support safe header updates.");
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

    for (std::uint64_t i = 0; i < sample_count; ++i) {
        const float clipped = std::clamp(samples[i], -1.0F, 1.0F);
        const auto pcm = static_cast<std::int16_t>(
            clipped <= -1.0F ? -32768 : std::lround(clipped * 32767.0F));
        write_u16_le(stream_, static_cast<std::uint16_t>(pcm));
    }
    if (!stream_) {
        set_error("Failed while writing audio samples.");
        return false;
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
    const bool okay = static_cast<bool>(stream_);
    stream_.close();
    finalized_ = okay;
    if (!okay) {
        set_error("Could not finalize the WAV header or flush the file.");
    }
    return okay;
}

void WavWriter::abort() noexcept {
    if (stream_.is_open()) {
        stream_.close();
    }
    finalized_ = true;
}

void WavWriter::set_error(const char* message) {
    last_error_ = message;
}

}  // namespace audio_studio
