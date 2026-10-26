#include "audio_studio/wav_writer.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
std::uint32_t read_u32(const std::vector<unsigned char>& bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U) |
           (static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
}
void test_wav_header_and_samples() {
    const std::string path = "audio_studio_test.wav";
    const std::array<float, 4> samples{-1.0F, -0.5F, 0.5F, 1.0F};
    {
        audio_studio::WavWriter writer;
        require(writer.open(path, {48000U, 2U}), "writer should open a valid WAV");
        require(writer.write_frames(samples.data(), 2U), "writer should accept stereo frames");
        require(writer.frames_written() == 2U, "writer should count audio frames");
        require(writer.finalize(), "writer should finalize header");
    }
    std::ifstream file(path, std::ios::binary);
    const std::vector<unsigned char> bytes{
        std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    require(bytes.size() == 52U, "two stereo PCM16 frames should produce a 52-byte WAV");
    require(std::string(bytes.begin(), bytes.begin() + 4) == "RIFF", "RIFF signature missing");
    require(std::string(bytes.begin() + 8, bytes.begin() + 12) == "WAVE", "WAVE signature missing");
    require(read_u32(bytes, 4U) == 44U, "RIFF size should be file size minus eight");
    require(read_u32(bytes, 40U) == 8U, "data chunk should contain eight bytes");
    require(bytes[44U] == 0x00U && bytes[45U] == 0x80U, "-1.0 should encode as PCM16 minimum");
    require(bytes[46U] == 0x00U && bytes[47U] == 0xc0U, "-0.5 should encode near PCM16 half scale");
    std::remove(path.c_str());
}
void test_rejects_invalid_input() {
    audio_studio::WavWriter writer;
    require(!writer.open("unused.wav", {48000U, 3U}), "unsupported channel count should fail");
    const std::string path = "audio_studio_invalid_test.wav";
    require(writer.open(path, {44100U, 1U}), "writer should open mono WAV");
    const std::array<float, 1> invalid{std::numeric_limits<float>::quiet_NaN()};
    require(!writer.write_frames(invalid.data(), 1U), "non-finite samples should be rejected");
    require(writer.finalize(), "writer should still safely finalize after rejected input");
    std::remove(path.c_str());
}
}
int main() {
    test_wav_header_and_samples();
    test_rejects_invalid_input();
    std::cout << "All WAV writer tests passed.\n";
    return EXIT_SUCCESS;
}
