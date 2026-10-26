#include "audio_studio/wav_processing.hpp"
#include "audio_studio/wav_reader.hpp"
#include "audio_studio/wav_writer.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

std::vector<unsigned char> load_bytes(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void test_processes_to_separate_file_and_preserves_source() {
    const std::string input = "audio_studio_processing_source.wav";
    const std::string output = "audio_studio_processing_result.wav";
    std::array<float, 4800> samples{};
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = i < 2400U ? 0.003F : 0.25F;
    }
    {
        audio_studio::WavWriter writer;
        require(writer.open(input, {48000U, 1U}), "create source WAV");
        require(writer.write_frames(samples.data(), samples.size()), "write source WAV");
        require(writer.finalize(), "finalize source WAV");
    }
    const auto original_bytes = load_bytes(input);
    std::string error;
    require(audio_studio::process_wav_with_noise_gate(input, output, {}, &error),
            "processing should create a separate WAV");
    require(error.empty(), "successful processing clears error");
    require(load_bytes(input) == original_bytes, "source bytes must remain unchanged");

    audio_studio::AudioBuffer original;
    audio_studio::AudioBuffer processed;
    require(audio_studio::WavReader::read(input, original), "read original");
    require(audio_studio::WavReader::read(output, processed), "read processed output");
    require(processed.format.sample_rate_hz == original.format.sample_rate_hz &&
            processed.format.channel_count == original.format.channel_count,
            "processing preserves format");
    require(processed.samples.size() == original.samples.size(),
            "processing preserves duration and sample count");
    audio_studio::AudioBuffer expected = original;
    audio_studio::NoiseGate reference_gate;
    require(reference_gate.process(expected), "reference gate processes the whole buffer");
    for (std::size_t i = 0; i < processed.samples.size(); ++i) {
        require(std::abs(processed.samples[i] - expected.samples[i]) <= (1.5F / 32767.0F),
                "streamed output must match continuous whole-buffer gate processing");
    }
    double original_quiet = 0.0;
    double processed_quiet = 0.0;
    for (std::size_t i = 0; i < 2400U; ++i) {
        original_quiet += std::abs(original.samples[i]);
        processed_quiet += std::abs(processed.samples[i]);
    }
    require(processed_quiet < original_quiet, "gate should attenuate the quiet region");
    std::remove(input.c_str());
    std::remove(output.c_str());
}

void test_refuses_to_overwrite_input() {
    const std::string input = "audio_studio_processing_no_overwrite.wav";
    const std::array<float, 8> samples{0.1F, 0.2F, 0.3F, 0.4F, 0.5F, 0.6F, 0.7F, 0.8F};
    {
        audio_studio::WavWriter writer;
        require(writer.open(input, {48000U, 1U}), "create no-overwrite source");
        require(writer.write_frames(samples.data(), samples.size()), "write no-overwrite source");
        require(writer.finalize(), "finalize no-overwrite source");
    }
    const auto before = load_bytes(input);
    std::string error;
    require(!audio_studio::process_wav_with_noise_gate(input, input, {}, &error),
            "must reject same input/output path");
    require(!error.empty(), "no-overwrite error should be described");
    require(load_bytes(input) == before, "rejected operation must preserve source");
    std::remove(input.c_str());
}
}  // namespace

int main() {
    test_processes_to_separate_file_and_preserves_source();
    test_refuses_to_overwrite_input();
    std::cout << "All WAV processing tests passed.\n";
    return EXIT_SUCCESS;
}
