#include "audio_studio/wav_reader.hpp"
#include "audio_studio/wav_writer.hpp"

#include <array>
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

std::vector<unsigned char> load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void save(const std::string& path, const std::vector<unsigned char>& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
}

void write_u32(std::vector<unsigned char>& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned int i = 0; i < 4U; ++i) {
        bytes[offset + i] = static_cast<unsigned char>((value >> (i * 8U)) & 0xffU);
    }
}

void test_round_trip_pcm16() {
    const std::string path = "audio_studio_reader_roundtrip.wav";
    const std::array<float, 4> samples{-1.0F, -0.5F, 0.5F, 1.0F};
    {
        audio_studio::WavWriter writer;
        require(writer.open(path, {44100U, 2U}), "writer should create stereo test WAV");
        require(writer.write_frames(samples.data(), 2U), "writer should write test samples");
        require(writer.finalize(), "writer should finalize test WAV");
    }
    audio_studio::AudioBuffer audio;
    std::string error;
    require(audio_studio::WavReader::read(path, audio, &error), "reader should decode writer output");
    require(error.empty(), "successful read should clear error");
    require(audio.format.sample_rate_hz == 44100U && audio.format.channel_count == 2U,
            "reader should preserve sample rate and channels");
    require(audio.samples.size() == samples.size(), "reader should preserve sample count");
    require(audio.samples[0] == -1.0F && audio.samples[3] == 1.0F,
            "reader should decode PCM16 endpoints");
    require(audio.samples[1] < -0.49F && audio.samples[1] > -0.51F,
            "reader should decode negative mid-scale sample");
    std::remove(path.c_str());
}

void test_chunk_order_and_padding() {
    const std::string original = "audio_studio_reader_chunks.wav";
    const std::string reordered = "audio_studio_reader_reordered.wav";
    const std::array<float, 2> samples{0.25F, -0.25F};
    {
        audio_studio::WavWriter writer;
        require(writer.open(original, {48000U, 1U}), "writer should create mono WAV");
        require(writer.write_frames(samples.data(), samples.size()), "writer should write mono data");
        require(writer.finalize(), "writer should finalize mono WAV");
    }
    const auto source = load(original);
    // Insert a legal odd-sized JUNK chunk (with its RIFF padding byte) before fmt.
    std::vector<unsigned char> bytes;
    bytes.insert(bytes.end(), source.begin(), source.begin() + 12);
    const std::array<unsigned char, 10> junk{'J','U','N','K',1,0,0,0,0x7f,0};
    bytes.insert(bytes.end(), junk.begin(), junk.end());
    bytes.insert(bytes.end(), source.begin() + 12, source.end());
    write_u32(bytes, 4U, static_cast<std::uint32_t>(bytes.size() - 8U));
    save(reordered, bytes);

    audio_studio::AudioBuffer audio;
    require(audio_studio::WavReader::read(reordered, audio),
            "reader should skip unknown padded chunks");
    require(audio.samples.size() == samples.size(), "chunk skipping should preserve audio");
    std::remove(original.c_str());
    std::remove(reordered.c_str());
}

void test_rejects_malformed_and_preserves_destination() {
    const std::string valid_path = "audio_studio_reader_valid.wav";
    const std::string bad_path = "audio_studio_reader_bad.wav";
    const std::array<float, 4> samples{0.1F, 0.2F, 0.3F, 0.4F};
    {
        audio_studio::WavWriter writer;
        require(writer.open(valid_path, {48000U, 1U}), "writer should create valid WAV");
        require(writer.write_frames(samples.data(), samples.size()), "writer should write valid samples");
        require(writer.finalize(), "writer should finalize valid WAV");
    }
    audio_studio::AudioBuffer audio;
    require(audio_studio::WavReader::read(valid_path, audio), "valid WAV should load");
    const auto old_samples = audio.samples;
    auto malformed = load(valid_path);
    malformed.resize(30U);
    save(bad_path, malformed);
    std::string error;
    require(!audio_studio::WavReader::read(bad_path, audio, &error),
            "truncated WAV should be rejected");
    require(!error.empty(), "failed read should explain the error");
    require(audio.samples == old_samples, "failed read must not change destination buffer");

    malformed = load(valid_path);
    write_u32(malformed, 4U, 0xffffffffU);
    save(bad_path, malformed);
    require(!audio_studio::WavReader::read(bad_path, audio),
            "oversized RIFF declaration should be rejected");
    std::remove(valid_path.c_str());
    std::remove(bad_path.c_str());
}
}  // namespace

int main() {
    test_round_trip_pcm16();
    test_chunk_order_and_padding();
    test_rejects_malformed_and_preserves_destination();
    std::cout << "All WAV reader tests passed.\n";
    return EXIT_SUCCESS;
}
