#include "audio_studio/wav_processing.hpp"

#include "audio_studio/wav_reader.hpp"
#include "audio_studio/wav_writer.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace audio_studio {
namespace {
constexpr std::uintmax_t kMaxInputBytes = 64U * 1024U * 1024U;

bool fail(std::string* error, const std::string& message) {
    if (error != nullptr) *error = message;
    return false;
}
}  // namespace

bool process_wav_with_noise_gate(const std::string& input_path,
                                 const std::string& output_path,
                                 NoiseGateSettings settings,
                                 std::string* error) {
    if (error != nullptr) error->clear();
    if (input_path.empty() || output_path.empty()) {
        return fail(error, "Input and output paths are required.");
    }
    if (input_path == output_path) {
        return fail(error, "Output must be a separate file; the original is never overwritten.");
    }
    if (!settings.is_valid()) return fail(error, "Noise-gate settings are invalid.");

    std::error_code ec;
    const auto input_absolute = std::filesystem::absolute(input_path, ec).lexically_normal();
    if (ec) return fail(error, "Could not resolve the input path.");
    const auto output_absolute = std::filesystem::absolute(output_path, ec).lexically_normal();
    if (ec) return fail(error, "Could not resolve the output path.");
    if (input_absolute == output_absolute) {
        return fail(error, "Output must be a separate file; the original is never overwritten.");
    }
    if (std::filesystem::exists(output_path, ec)) {
        if (ec) return fail(error, "Could not inspect the output path.");
        const bool same_file = std::filesystem::equivalent(input_path, output_path, ec);
        if (!ec && same_file) {
            return fail(error, "Output refers to the original file; refusing to overwrite it.");
        }
        if (ec) ec.clear();
        return fail(error, "Output file already exists; choose a new destination.");
    }
    if (ec) return fail(error, "Could not inspect the output path.");

    const auto input_size = std::filesystem::file_size(input_path, ec);
    if (ec) return fail(error, "Could not determine the input file size.");
    if (input_size > kMaxInputBytes) {
        return fail(error, "This first processing version supports WAV files up to 64 MiB. The original is unchanged.");
    }

    AudioBuffer audio;
    std::string reader_error;
    if (!WavReader::read(input_path, audio, &reader_error)) {
        return fail(error, "Could not read WAV: " + reader_error);
    }

    NoiseGate gate(settings);
    if (!gate.process(audio)) return fail(error, "Noise gate rejected the decoded audio.");

    WavWriter writer;
    if (!writer.open(output_path, audio.format)) {
        return fail(error, "Could not create processed WAV: " + writer.last_error());
    }
    if (!writer.write_frames(audio.samples.data(), audio.frame_count())) {
        const std::string message = "Could not write processed WAV: " + writer.last_error();
        writer.abort();
        std::filesystem::remove(output_path, ec);
        return fail(error, message);
    }
    if (!writer.finalize()) {
        const std::string message = "Could not finalize processed WAV: " + writer.last_error();
        writer.abort();
        std::filesystem::remove(output_path, ec);
        return fail(error, message);
    }
    return true;
}

}  // namespace audio_studio
