#include "audio_studio/wav_recovery.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
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

void write_u16(std::array<unsigned char, 44U>& h, std::size_t offset,
               unsigned int value) {
    h[offset] = static_cast<unsigned char>(value & 0xffU);
    h[offset + 1U] = static_cast<unsigned char>((value >> 8U) & 0xffU);
}
void write_u32(std::array<unsigned char, 44U>& h, std::size_t offset,
               unsigned int value) {
    for (unsigned int i = 0U; i < 4U; ++i) {
        h[offset + i] = static_cast<unsigned char>((value >> (i * 8U)) & 0xffU);
    }
}
unsigned int read_u32(const std::vector<unsigned char>& bytes, std::size_t offset) {
    return static_cast<unsigned int>(bytes[offset]) |
           (static_cast<unsigned int>(bytes[offset + 1U]) << 8U) |
           (static_cast<unsigned int>(bytes[offset + 2U]) << 16U) |
           (static_cast<unsigned int>(bytes[offset + 3U]) << 24U);
}
std::array<unsigned char, 44U> zero_length_header() {
    std::array<unsigned char, 44U> h{};
    const char* riff = "RIFF";
    const char* wave = "WAVE";
    const char* fmt = "fmt ";
    const char* data = "data";
    std::copy(riff, riff + 4, h.begin());
    std::copy(wave, wave + 4, h.begin() + 8);
    std::copy(fmt, fmt + 4, h.begin() + 12);
    std::copy(data, data + 4, h.begin() + 36);
    write_u32(h, 16U, 16U);
    write_u16(h, 20U, 1U);
    write_u16(h, 22U, 1U);
    write_u32(h, 24U, 48000U);
    write_u32(h, 28U, 96000U);
    write_u16(h, 32U, 2U);
    write_u16(h, 34U, 16U);
    // Simulates a crash before the writer rewrites RIFF/data lengths.
    return h;
}
void create_partial(const std::string& path, const std::vector<unsigned char>& payload) {
    auto header = zero_length_header();
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(header.data()),
               static_cast<std::streamsize>(header.size()));
    if (!payload.empty()) {
        file.write(reinterpret_cast<const char*>(payload.data()),
                   static_cast<std::streamsize>(payload.size()));
    }
}
void test_recovers_valid_payload_and_repairs_header() {
    const std::string partial = "recovery_sample.wav.partial-17";
    const std::string output = "recovery_sample.wav";
    const std::vector<unsigned char> payload{0x01U, 0x02U, 0x03U, 0x04U};
    create_partial(partial, payload);
    std::string error;
    require(audio_studio::recover_partial_wav(partial, output, &error),
            "valid abandoned WAV should recover");
    require(!std::filesystem::exists(partial), "successful recovery should move the partial file");
    std::ifstream file(output, std::ios::binary);
    const std::vector<unsigned char> bytes{
        std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    require(bytes.size() == 48U, "recovered WAV should retain all payload bytes");
    require(read_u32(bytes, 4U) == 40U, "recovered RIFF length should match actual file");
    require(read_u32(bytes, 40U) == 4U, "recovered data length should match payload");
    require(std::equal(payload.begin(), payload.end(), bytes.begin() + 44),
            "recovery must not change captured audio samples");
    std::remove(output.c_str());
}
void test_rejects_partial_frame_without_mutating_source() {
    const std::string partial = "recovery_misaligned.wav.partial-18";
    const std::string output = "recovery_misaligned.wav";
    create_partial(partial, {0x11U, 0x22U, 0x33U});
    std::string error;
    require(!audio_studio::recover_partial_wav(partial, output, &error),
            "recovery must reject a payload ending inside a PCM frame");
    require(std::filesystem::exists(partial), "invalid partial must be retained for manual inspection");
    require(!std::filesystem::exists(output), "invalid partial must not become a final recording");
    std::remove(partial.c_str());
}
void test_never_overwrites_existing_recording() {
    const std::string partial = "recovery_existing.wav.partial-19";
    const std::string output = "recovery_existing.wav";
    create_partial(partial, {0x01U, 0x02U});
    const std::array<unsigned char, 3U> existing{0xaaU, 0xbbU, 0xccU};
    {
        std::ofstream file(output, std::ios::binary);
        file.write(reinterpret_cast<const char*>(existing.data()),
                   static_cast<std::streamsize>(existing.size()));
    }
    std::string error;
    require(!audio_studio::recover_partial_wav(partial, output, &error),
            "recovery must refuse an existing final destination");
    std::ifstream file(output, std::ios::binary);
    const std::vector<unsigned char> actual{
        std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    require(actual == std::vector<unsigned char>(existing.begin(), existing.end()),
            "existing recording must remain byte-for-byte unchanged");
    require(std::filesystem::exists(partial), "source partial must remain after destination conflict");
    std::remove(partial.c_str());
    std::remove(output.c_str());
}
void test_rejects_unrelated_filename() {
    const std::string partial = "not_owned.tmp";
    const std::string output = "not_owned";
    create_partial(partial, {0x01U, 0x02U});
    std::string error;
    require(!audio_studio::recover_partial_wav(partial, output, &error),
            "recovery must only accept writer-owned partial filename pattern");
    require(std::filesystem::exists(partial), "unrecognized source must be retained");
    std::remove(partial.c_str());
}
}  // namespace

int main() {
    test_recovers_valid_payload_and_repairs_header();
    test_rejects_partial_frame_without_mutating_source();
    test_never_overwrites_existing_recording();
    test_rejects_unrelated_filename();
    std::cout << "All WAV recovery tests passed.\n";
    return EXIT_SUCCESS;
}
