#include "audio_studio/wav_processing.hpp"

#include "audio_studio/wav_writer.hpp"
#include "audio_studio/audio_decoder.hpp"
#include "audio_studio/dsp_effects.hpp"
#include "noise_cancellation.h"

#include <algorithm>
#include <atomic>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <exception>
#include <string>
#include <vector>

namespace audio_studio {
namespace {
constexpr std::size_t kFramesPerBlock = 4096U;

bool fail(std::string* error, const std::string& message) {
    if (error != nullptr) *error = message;
    return false;
}

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

bool read_exact(std::ifstream& stream, unsigned char* destination, std::size_t size) {
    stream.read(reinterpret_cast<char*>(destination), static_cast<std::streamsize>(size));
    return static_cast<bool>(stream);
}

bool report_progress(const std::function<void(std::uint64_t, std::uint64_t)>& progress,
                     std::uint64_t processed, std::uint64_t total,
                     std::string* error) {
    if (!progress) return true;
    try {
        progress(processed, total);
        return true;
    } catch (const std::exception& exception) {
        return fail(error, std::string("Progress callback failed: ") + exception.what());
    } catch (...) {
        return fail(error, "Progress callback failed with an unknown exception.");
    }
}

bool tag_is(const std::array<unsigned char, 4U>& tag, const char* expected) noexcept {
    return tag[0] == static_cast<unsigned char>(expected[0]) &&
           tag[1] == static_cast<unsigned char>(expected[1]) &&
           tag[2] == static_cast<unsigned char>(expected[2]) &&
           tag[3] == static_cast<unsigned char>(expected[3]);
}

bool parse_wav_header(std::ifstream& stream, std::uint64_t file_size,
                      AudioFormat& format, std::uint64_t& data_offset,
                      std::uint32_t& data_size, std::uint16_t& encoding_out,
                      std::uint16_t& bits_per_sample_out, std::uint16_t& block_align_out,
                      std::string* error) {
    if (file_size < 12U) return fail(error, "WAV file is too small to contain a RIFF header.");

    std::array<unsigned char, 12U> header{};
    stream.seekg(0, std::ios::beg);
    if (!read_exact(stream, header.data(), header.size())) {
        return fail(error, "Could not read the WAV header.");
    }
    const std::array<unsigned char, 4U> riff{header[0], header[1], header[2], header[3]};
    const std::array<unsigned char, 4U> wave{header[8], header[9], header[10], header[11]};
    if (!tag_is(riff, "RIFF") || !tag_is(wave, "WAVE")) {
        return fail(error, "File is not a RIFF/WAVE file.");
    }

    const std::uint64_t riff_end = static_cast<std::uint64_t>(read_u32_le(header.data() + 4U)) + 8U;
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
    std::uint64_t cursor = 12U;

    while (cursor + 8U <= riff_end) {
        stream.seekg(static_cast<std::streamoff>(cursor), std::ios::beg);
        std::array<unsigned char, 8U> chunk_header{};
        if (!read_exact(stream, chunk_header.data(), chunk_header.size())) {
            return fail(error, "WAV contains an incomplete chunk header.");
        }
        const std::array<unsigned char, 4U> chunk_tag{
            chunk_header[0], chunk_header[1], chunk_header[2], chunk_header[3]};
        const std::uint32_t chunk_size = read_u32_le(chunk_header.data() + 4U);
        const std::uint64_t payload = cursor + 8U;
        const std::uint64_t payload_end = payload + chunk_size;
        const std::uint64_t padded_end = payload_end + (chunk_size & 1U);
        if (payload_end < payload || padded_end < payload_end || padded_end > riff_end) {
            return fail(error, "WAV contains a truncated or out-of-bounds chunk.");
        }

        if (tag_is(chunk_tag, "fmt ")) {
            if (have_format || chunk_size < 16U) {
                return fail(error, "WAV format chunk is duplicated or too short.");
            }
            std::array<unsigned char, 16U> fmt{};
            if (!read_exact(stream, fmt.data(), fmt.size())) {
                return fail(error, "Could not read the WAV format chunk.");
            }
            encoding = read_u16_le(fmt.data());
            channels = read_u16_le(fmt.data() + 2U);
            sample_rate = read_u32_le(fmt.data() + 4U);
            const std::uint32_t byte_rate = read_u32_le(fmt.data() + 8U);
            block_align = read_u16_le(fmt.data() + 12U);
            bits_per_sample = read_u16_le(fmt.data() + 14U);
            // WAVE_FORMAT_EXTENSIBLE wraps the actual subformat in a GUID.
            // Resolve only the canonical PCM/IEEE-float GUIDs; do not guess codecs.
            if (encoding == 0xfffeU) {
                if (chunk_size < 40U) {
                    return fail(error, "WAVE_FORMAT_EXTENSIBLE format chunk is too short.");
                }
                std::array<unsigned char, 24U> extension{};
                stream.seekg(static_cast<std::streamoff>(payload + 16U), std::ios::beg);
                if (!read_exact(stream, extension.data(), extension.size())) {
                    return fail(error, "Could not read the WAVE_FORMAT_EXTENSIBLE fields.");
                }
                const std::uint16_t extension_size = read_u16_le(extension.data());
                const std::uint16_t valid_bits = read_u16_le(extension.data() + 2U);
                const std::uint32_t subformat = read_u32_le(extension.data() + 8U);
                const std::array<unsigned char, 12U> guid_tail{
                    extension[12], extension[13], extension[14], extension[15],
                    extension[16], extension[17], extension[18], extension[19],
                    extension[20], extension[21], extension[22], extension[23]};
                const std::array<unsigned char, 12U> canonical_tail{
                    0x00U, 0x00U, 0x10U, 0x00U, 0x80U, 0x00U,
                    0x00U, 0xaaU, 0x00U, 0x38U, 0x9bU, 0x71U};
                if (extension_size < 22U || extension_size > chunk_size - 18U ||
                    valid_bits != bits_per_sample || guid_tail != canonical_tail ||
                    (subformat != 1U && subformat != 3U)) {
                    return fail(error, "Unsupported WAVE_FORMAT_EXTENSIBLE subformat or valid-bit layout.");
                }
                encoding = static_cast<std::uint16_t>(subformat);
            }
            const bool pcm_supported = encoding == 1U &&
                (bits_per_sample == 8U || bits_per_sample == 16U ||
                 bits_per_sample == 24U || bits_per_sample == 32U);
            const bool float_supported = encoding == 3U && bits_per_sample == 32U;
            const std::uint16_t bytes_per_sample = static_cast<std::uint16_t>(bits_per_sample / 8U);
            if ((!pcm_supported && !float_supported) ||
                (channels != 1U && channels != 2U) ||
                sample_rate < 8000U || sample_rate > 192000U ||
                block_align != channels * bytes_per_sample ||
                byte_rate != sample_rate * static_cast<std::uint32_t>(block_align)) {
                return fail(error, "Supported WAV input is mono/stereo PCM 8/16/24/32-bit or IEEE float32, including standard WAVE_FORMAT_EXTENSIBLE wrappers.");
            }
            have_format = true;
        } else if (tag_is(chunk_tag, "data")) {
            if (have_data) return fail(error, "WAV contains multiple data chunks.");
            data_offset = payload;
            data_size = chunk_size;
            have_data = true;
        }
        cursor = padded_end;
    }

    if (cursor != riff_end) return fail(error, "WAV contains an incomplete chunk header.");
    if (!have_format || !have_data) {
        return fail(error, "WAV is missing its format or data chunk.");
    }
    if (data_size == 0U || data_size % block_align != 0U) {
        return fail(error, "WAV audio data is empty or not frame-aligned.");
    }
    if (data_offset + data_size > riff_end) {
        return fail(error, "WAV audio data extends beyond the RIFF container.");
    }

    format = {sample_rate, channels};
    encoding_out = encoding;
    bits_per_sample_out = bits_per_sample;
    block_align_out = block_align;
    return true;
}

bool decode_sample(const unsigned char* bytes, std::uint16_t encoding,
                   std::uint16_t bits_per_sample, float& sample) noexcept {
    if (encoding == 3U && bits_per_sample == 32U) {
        std::uint32_t raw = read_u32_le(bytes);
        std::memcpy(&sample, &raw, sizeof(sample));
        return std::isfinite(sample);
    }
    if (encoding != 1U) return false;
    switch (bits_per_sample) {
    case 8U:
        sample = (static_cast<int>(bytes[0]) - 128) / 128.0F;
        return true;
    case 16U: {
        const auto raw = read_u16_le(bytes);
        const auto value = static_cast<std::int16_t>(raw);
        sample = value < 0 ? static_cast<float>(value) / 32768.0F
                           : static_cast<float>(value) / 32767.0F;
        return true;
    }
    case 24U: {
        std::int32_t raw = static_cast<std::int32_t>(bytes[0]) |
            (static_cast<std::int32_t>(bytes[1]) << 8U) |
            (static_cast<std::int32_t>(bytes[2]) << 16U);
        const std::int64_t value = (raw & 0x800000) != 0
            ? static_cast<std::int64_t>(raw) - 0x1000000LL
            : static_cast<std::int64_t>(raw);
        sample = value < 0 ? static_cast<float>(value / 8388608.0)
                           : static_cast<float>(value / 8388607.0);
        return true;
    }
    case 32U: {
        const std::uint32_t raw = read_u32_le(bytes);
        const std::int64_t value = raw >= 0x80000000U
            ? static_cast<std::int64_t>(raw) - 0x100000000LL
            : static_cast<std::int64_t>(raw);
        sample = value < 0 ? static_cast<float>(value / 2147483648.0)
                           : static_cast<float>(value / 2147483647.0);
        return true;
    }
    default:
        return false;
    }
}
}  // namespace

bool process_wav_with_noise_gate(const std::string& input_path,
                                 const std::string& output_path,
                                 NoiseGateSettings settings,
                                 std::string* error,
                                 const std::atomic<bool>* cancel_requested,
                                 std::function<void(std::uint64_t, std::uint64_t)> progress,
                                 DspEffectsSettings dsp_settings,
                                 ProcessingEngine engine,
                                 float denoiser_strength,
                                 const std::string& model_directory) {
    if (error != nullptr) error->clear();
    if (input_path.empty() || output_path.empty()) {
        return fail(error, "Input and output paths are required.");
    }
    if (input_path == output_path) {
        return fail(error, "Output must be a separate file; the original is never overwritten.");
    }
    if (engine != ProcessingEngine::NoiseGate && engine != ProcessingEngine::Rnnoise &&
        engine != ProcessingEngine::DeepFilterNet3) {
        return fail(error, "Unsupported audio processing engine.");
    }
    if (engine == ProcessingEngine::NoiseGate && !settings.is_valid()) {
        return fail(error, "Noise-gate settings are invalid.");
    }
    if (!std::isfinite(denoiser_strength) || denoiser_strength < 0.0F || denoiser_strength > 1.0F) {
        return fail(error, "Denoiser strength must be between 0 and 1.");
    }

    const auto input_fs = std::filesystem::u8path(input_path);
    const auto output_fs = std::filesystem::u8path(output_path);
    std::error_code ec;
    const auto input_absolute = std::filesystem::absolute(input_fs, ec).lexically_normal();
    if (ec) return fail(error, "Could not resolve the input path.");
    const auto output_absolute = std::filesystem::absolute(output_fs, ec).lexically_normal();
    if (ec) return fail(error, "Could not resolve the output path.");
    if (input_absolute == output_absolute) {
        return fail(error, "Output must be a separate file; the original is never overwritten.");
    }
    if (!std::filesystem::exists(input_fs, ec) || ec) {
        return fail(error, "Could not inspect the input path.");
    }
    if (std::filesystem::exists(output_fs, ec)) {
        if (ec) return fail(error, "Could not inspect the output path.");
        const bool same_file = std::filesystem::equivalent(input_fs, output_fs, ec);
        if (!ec && same_file) {
            return fail(error, "Output refers to the original file; refusing to overwrite it.");
        }
        if (ec) ec.clear();
        return fail(error, "Output file already exists; choose a new destination.");
    }
    if (ec) return fail(error, "Could not inspect the output path.");

    const auto input_size = std::filesystem::file_size(input_fs, ec);
    if (ec) return fail(error, "Could not determine the input file size.");
    std::ifstream input(input_fs, std::ios::binary);
    if (!input.is_open()) return fail(error, "Could not open the input WAV file.");

    AudioFormat format{};
    std::uint64_t data_offset = 0U;
    std::uint32_t data_size = 0U;
    std::uint16_t encoding = 0U, bits_per_sample = 0U, block_align = 0U;
    if (!parse_wav_header(input, input_size, format, data_offset, data_size,
                          encoding, bits_per_sample, block_align, error)) return false;

    NoiseGate gate(settings);
    NcMultiHandle* denoiser_raw = nullptr;
    std::unique_ptr<NcMultiHandle, decltype(&nc_destroy_multi)> denoiser(nullptr, &nc_destroy_multi);
    if (engine == ProcessingEngine::Rnnoise || engine == ProcessingEngine::DeepFilterNet3) {
        const NcEngine native_engine = engine == ProcessingEngine::Rnnoise
            ? NC_ENGINE_RNNOISE : NC_ENGINE_DEEPFILTERNET;
        const char* model_path = model_directory.empty() ? nullptr : model_directory.c_str();
        const auto status = nc_create_multi_with_rate(
            native_engine, model_path, denoiser_strength, format.sample_rate_hz,
            format.channel_count, &denoiser_raw);
        if (status != NC_OK || denoiser_raw == nullptr) {
            return fail(error, std::string(engine == ProcessingEngine::Rnnoise ? "Could not initialize RNNoise: " : "Could not initialize DeepFilterNet3 (BETA): ") +
                nc_status_string(status));
        }
        denoiser.reset(denoiser_raw);
    }
    DspEffects dsp_effects;
    if (!dsp_effects.configure(format, dsp_settings, error)) return false;
    WavWriter writer;
    if (!writer.open_path(output_fs, format)) {
        return fail(error, "Could not create processed WAV: " + writer.last_error());
    }

    const std::size_t channels = format.channel_count;
    const std::size_t bytes_per_sample = bits_per_sample / 8U;
    const std::size_t bytes_per_frame = block_align;
    const std::uint64_t total_frames = data_size / bytes_per_frame;
    std::vector<unsigned char> raw(kFramesPerBlock * bytes_per_frame);
    AudioBuffer block;
    block.format = format;
    block.samples.resize(kFramesPerBlock * channels);
    input.clear();
    input.seekg(static_cast<std::streamoff>(data_offset), std::ios::beg);
    if (!input) {
        writer.abort();

        return fail(error, "Could not seek to WAV audio data.");
    }

    std::uint64_t frames_remaining = total_frames;
    if (!report_progress(progress, 0U, total_frames, error)) {
        writer.abort();
        return false;
    }
    while (frames_remaining > 0U) {
        if (cancel_requested != nullptr && cancel_requested->load(std::memory_order_acquire)) {
            writer.abort();

            return fail(error, "Processing cancelled by user.");
        }
        const std::size_t frames = static_cast<std::size_t>(
            std::min<std::uint64_t>(frames_remaining, kFramesPerBlock));
        const std::size_t sample_count = frames * channels;
        const std::size_t bytes_to_read = frames * bytes_per_frame;
        if (!read_exact(input, raw.data(), bytes_to_read)) {
            writer.abort();

            return fail(error, "Could not read the complete WAV audio data.");
        }

        block.samples.resize(sample_count);
        for (std::size_t i = 0U; i < sample_count; ++i) {
            if (!decode_sample(raw.data() + i * bytes_per_sample, encoding,
                               bits_per_sample, block.samples[i])) {
                writer.abort();
                return fail(error, "WAV contains a non-finite or unsupported audio sample.");
            }
        }
        if (engine == ProcessingEngine::NoiseGate) {
            if (!gate.process(block)) {
                writer.abort();
                return fail(error, "Noise gate rejected a decoded audio block.");
            }
        } else {
            const auto status = nc_process_interleaved(
                denoiser.get(), block.samples.data(), block.samples.data(), frames);
            if (status != NC_OK) {
                writer.abort();
                return fail(error, std::string(engine == ProcessingEngine::Rnnoise ? "RNNoise processing failed: " : "DeepFilterNet3 (BETA) processing failed: ") +
                    nc_status_string(status));
            }
        }
        if (!dsp_effects.process(block, error)) {
            writer.abort();
            return false;
        }
        if (!writer.write_frames(block.samples.data(), frames)) {
            const std::string message = "Could not write processed WAV: " + writer.last_error();
            writer.abort();

            return fail(error, message);
        }
        frames_remaining -= frames;
        if (!report_progress(progress, total_frames - frames_remaining, total_frames, error)) {
            writer.abort();
            return false;
        }
    }

    if (cancel_requested != nullptr && cancel_requested->load(std::memory_order_acquire)) {
        writer.abort();

        return fail(error, "Processing cancelled by user.");
    }
    if (denoiser) {
        std::vector<float> tail(kFramesPerBlock * channels);
        while (true) {
            if (cancel_requested != nullptr && cancel_requested->load(std::memory_order_acquire)) {
                writer.abort();
                return fail(error, "Processing cancelled by user.");
            }
            std::size_t tail_frames = 0U;
            const auto status = nc_flush_interleaved(
                denoiser.get(), tail.data(), kFramesPerBlock, &tail_frames);
            if (status != NC_OK) {
                writer.abort();
                return fail(error, std::string(engine == ProcessingEngine::Rnnoise ? "RNNoise tail flush failed: " : "DeepFilterNet3 (BETA) tail flush failed: ") +
                    nc_status_string(status));
            }
            if (tail_frames == 0U) break;
            if (tail_frames > kFramesPerBlock ||
                !writer.write_frames(tail.data(), tail_frames)) {
                const std::string message = engine == ProcessingEngine::Rnnoise ? "Could not write RNNoise tail: " : "Could not write DeepFilterNet3 (BETA) tail: " + writer.last_error();
                writer.abort();
                return fail(error, message);
            }
        }
    }
    if (!writer.finalize()) {
        const std::string message = "Could not finalize processed WAV: " + writer.last_error();
        writer.abort();

        return fail(error, message);
    }
    return true;
}


bool analyze_wav_waveform(const std::string& input_path,
                          std::size_t bucket_count,
                          std::vector<WaveformPeak>& peaks,
                          AudioFormat* format_out,
                          std::uint64_t* total_frames_out,
                          std::string* error) {
    if (error != nullptr) error->clear();
    peaks.clear();
    if (input_path.empty()) return fail(error, "An input WAV path is required.");
    if (bucket_count == 0U || bucket_count > 1000000U) {
        return fail(error, "Waveform bucket count must be between 1 and 1000000.");
    }

    auto decoder = open_audio_decoder(input_path, error);
    if (!decoder) return false;

    const AudioFormat format = decoder->format();
    const std::size_t channels = format.channel_count;
    const std::uint64_t frames = decoder->total_frames();
    if (frames == 0U || (channels != 1U && channels != 2U)) {
        return fail(error, "Audio decoder returned an empty or unsupported audio stream.");
    }

    const std::size_t output_buckets = static_cast<std::size_t>(
        std::min<std::uint64_t>(frames, static_cast<std::uint64_t>(bucket_count)));
    std::vector<WaveformPeak> result(output_buckets);
    for (auto& peak : result) {
        peak.min_left = 1.0F;
        peak.max_left = -1.0F;
        peak.min_right = 1.0F;
        peak.max_right = -1.0F;
    }

    std::vector<float> block(kFramesPerBlock * channels);
    std::uint64_t frame_index = 0U;
    while (frame_index < frames) {
        const std::size_t requested = static_cast<std::size_t>(
            std::min<std::uint64_t>(frames - frame_index, kFramesPerBlock));
        std::size_t frames_read = 0U;
        if (!decoder->read_frames(block.data(), requested, frames_read, error)) {
            peaks.clear();
            return false;
        }
        if (frames_read == 0U || frames_read > requested) {
            peaks.clear();
            return fail(error, "Audio decoder returned an invalid waveform block.");
        }
        for (std::size_t i = 0U; i < frames_read; ++i, ++frame_index) {
            float left = block[i * channels];
            float right = channels == 2U ? block[i * channels + 1U] : left;
            left = std::clamp(left, -1.0F, 1.0F);
            right = std::clamp(right, -1.0F, 1.0F);
            const std::size_t bucket = static_cast<std::size_t>(
                (frame_index * static_cast<std::uint64_t>(output_buckets)) / frames);
            auto& peak = result[std::min(bucket, output_buckets - 1U)];
            peak.min_left = std::min(peak.min_left, left);
            peak.max_left = std::max(peak.max_left, left);
            peak.min_right = std::min(peak.min_right, right);
            peak.max_right = std::max(peak.max_right, right);
        }
    }

    peaks = std::move(result);
    if (format_out != nullptr) *format_out = format;
    if (total_frames_out != nullptr) *total_frames_out = frames;
    return true;
}

bool trim_wav(const std::string& input_path,
              const std::string& output_path,
              std::uint64_t start_frame,
              std::uint64_t end_frame,
              std::string* error) {
    if (error != nullptr) error->clear();
    if (input_path.empty() || output_path.empty()) {
        return fail(error, "Input and output paths are required.");
    }
    if (start_frame >= end_frame) {
        return fail(error, "Trim end must be after trim start.");
    }

    const auto input_fs = std::filesystem::u8path(input_path);
    const auto output_fs = std::filesystem::u8path(output_path);
    std::error_code ec;
    const auto input_absolute = std::filesystem::absolute(input_fs, ec).lexically_normal();
    if (ec) return fail(error, "Could not resolve the input path.");
    const auto output_absolute = std::filesystem::absolute(output_fs, ec).lexically_normal();
    if (ec) return fail(error, "Could not resolve the output path.");
    if (input_absolute == output_absolute) {
        return fail(error, "Output must be a separate file; the original is never overwritten.");
    }
    if (!std::filesystem::exists(input_fs, ec) || ec) {
        return fail(error, "Could not inspect the input path.");
    }
    if (std::filesystem::exists(output_fs, ec)) {
        if (ec) return fail(error, "Could not inspect the output path.");
        const bool same_file = std::filesystem::equivalent(input_fs, output_fs, ec);
        if (!ec && same_file) {
            return fail(error, "Output refers to the original file; refusing to overwrite it.");
        }
        return fail(error, "Output file already exists; choose a new destination.");
    }
    if (ec) return fail(error, "Could not inspect the output path.");

    const auto input_size = std::filesystem::file_size(input_fs, ec);
    if (ec) return fail(error, "Could not determine the input file size.");
    std::ifstream input(input_fs, std::ios::binary);
    if (!input.is_open()) return fail(error, "Could not open the input WAV file.");

    AudioFormat format{};
    std::uint64_t data_offset = 0U;
    std::uint32_t data_size = 0U;
    std::uint16_t encoding = 0U, bits_per_sample = 0U, block_align = 0U;
    if (!parse_wav_header(input, input_size, format, data_offset, data_size,
                          encoding, bits_per_sample, block_align, error)) return false;

    const std::uint64_t bytes_per_frame = block_align;
    const std::uint64_t total_frames = data_size / bytes_per_frame;
    if (end_frame > total_frames) {
        return fail(error, "Trim range extends beyond the end of the WAV file.");
    }

    WavWriter writer;
    if (!writer.open_path(output_fs, format)) {
        return fail(error, "Could not create trimmed WAV: " + writer.last_error());
    }

    const std::uint64_t first_byte = data_offset + start_frame * bytes_per_frame;
    input.clear();
    input.seekg(static_cast<std::streamoff>(first_byte), std::ios::beg);
    if (!input) {
        writer.abort();
        return fail(error, "Could not seek to the selected trim start.");
    }

    const std::size_t channels = format.channel_count;
    const std::size_t bytes_per_sample = bits_per_sample / 8U;
    const std::size_t bytes_per_frame_size = block_align;
    std::vector<unsigned char> raw(kFramesPerBlock * bytes_per_frame_size);
    std::vector<float> samples(kFramesPerBlock * channels);
    std::uint64_t remaining = end_frame - start_frame;
    while (remaining > 0U) {
        const std::size_t frames = static_cast<std::size_t>(
            std::min<std::uint64_t>(remaining, kFramesPerBlock));
        const std::size_t sample_count = frames * channels;
        const std::size_t bytes_to_read = frames * bytes_per_frame_size;
        if (!read_exact(input, raw.data(), bytes_to_read)) {
            writer.abort();
            return fail(error, "Could not read the selected WAV trim range.");
        }
        for (std::size_t i = 0U; i < sample_count; ++i) {
            if (!decode_sample(raw.data() + i * bytes_per_sample, encoding,
                               bits_per_sample, samples[i])) {
                writer.abort();
                return fail(error, "WAV contains a non-finite or unsupported audio sample.");
            }
        }
        if (!writer.write_frames(samples.data(), frames)) {
            const std::string message = "Could not write trimmed WAV: " + writer.last_error();
            writer.abort();
            return fail(error, message);
        }
        remaining -= frames;
    }

    if (!writer.finalize()) {
        const std::string message = "Could not finalize trimmed WAV: " + writer.last_error();
        writer.abort();
        return fail(error, message);
    }
    return true;
}

}  // namespace audio_studio
