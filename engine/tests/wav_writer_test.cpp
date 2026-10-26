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

void test_repeated_long_recordings() {
    const std::vector<float> block(480U, 0.125F);
    for (int session = 0; session < 3; ++session) {
        const std::string path = "audio_studio_session_" + std::to_string(session) + ".wav";
        {
            audio_studio::WavWriter writer;
            require(writer.open(path, {48000U, 1U}), "each recording session should open cleanly");
            for (int chunk = 0; chunk < 100; ++chunk) {
                require(writer.write_frames(block.data(), block.size()),
                        "long recording chunks should be written without errors");
            }
            require(writer.frames_written() == 48000U,
                    "one second of mono 48 kHz audio should count 48000 frames");
            require(writer.finalize(), "each recording session should finalize cleanly");
            require(!writer.write_frames(block.data(), block.size()),
                    "writes after finalization must be rejected");
        }

        std::ifstream file(path, std::ios::binary);
        const std::vector<unsigned char> bytes{
            std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        require(bytes.size() == 44U + 48000U * 2U,
                "long session should have a complete PCM16 payload");
        require(read_u32(bytes, 4U) == bytes.size() - 8U,
                "long session RIFF size should match the actual file");
        require(read_u32(bytes, 40U) == 48000U * 2U,
                "long session data size should match all written samples");
        std::remove(path.c_str());
    }
}

}
int main() {
    test_wav_header_and_samples();
    test_rejects_invalid_input();
    test_repeated_long_recordings();
    std::cout << "All WAV writer tests passed.\n";
    return EXIT_SUCCESS;
}
