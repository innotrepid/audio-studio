#include "audio_studio/wav_writer.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <filesystem>
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
bool has_partial_file_for(const std::string& path) {
    const auto prefix = std::filesystem::path(path).filename().string() + ".partial-";
    for (const auto& entry : std::filesystem::directory_iterator(".")) {
        if (entry.path().filename().string().rfind(prefix, 0U) == 0U) return true;
    }
    return false;
}
void test_wav_header_and_samples() {
    const std::string path = "audio_studio_test.wav";
    const std::array<float, 4> samples{-1.0F, -0.5F, 0.5F, 1.0F};
    {
        audio_studio::WavWriter writer;
        require(writer.open(path, {48000U, 2U}), "writer should open a valid WAV");
        require(writer.write_frames(samples.data(), 2U), "writer should accept stereo frames");
        require(writer.frames_written() == 2U, "writer should count audio frames");
        require(!std::ifstream(path, std::ios::binary).good(),
                "final destination must stay invisible while recording is in progress");
        require(writer.finalize(), "writer should finalize header");
        require(std::ifstream(path, std::ios::binary).good(),
                "final destination should appear after successful finalization");
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
void test_existing_destination_is_never_overwritten() {
    const std::string path = "audio_studio_existing_destination.wav";
    const std::array<unsigned char, 5> original{1U, 2U, 3U, 4U, 5U};
    {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(original.data()),
                   static_cast<std::streamsize>(original.size()));
    }
    {
        audio_studio::WavWriter writer;
        require(!writer.open(path, {48000U, 1U}),
                "writer must reject an existing final destination");
    }
    std::ifstream file(path, std::ios::binary);
    const std::vector<unsigned char> actual{
        std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    require(actual == std::vector<unsigned char>(original.begin(), original.end()),
            "existing destination bytes must remain unchanged");
    std::remove(path.c_str());
}

void test_rejects_invalid_input() {
    audio_studio::WavWriter writer;
    require(!writer.open("unused.wav", {48000U, 3U}), "unsupported channel count should fail");
    const std::string path = "audio_studio_invalid_test.wav";
    require(writer.open(path, {44100U, 1U}), "writer should open mono WAV");

    const std::array<float, 3> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()};
    for (const float sample : invalid) {
        require(!writer.write_frames(&sample, 1U),
                "NaN and positive/negative infinity samples should be rejected");
        require(writer.frames_written() == 0U,
                "rejecting a non-finite sample must not advance the frame count");
    }

    const std::array<float, 1> valid{0.25F};
    require(writer.write_frames(valid.data(), valid.size()),
            "writer should accept valid samples after rejecting invalid input");
    require(writer.frames_written() == 1U,
            "only successfully written frames should be counted");
    require(writer.finalize(), "writer should still safely finalize after rejected input");

    std::ifstream file(path, std::ios::binary);
    const std::vector<unsigned char> bytes{
        std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    require(bytes.size() == 46U, "recovered writer should contain only the valid sample");
    require(read_u32(bytes, 40U) == 2U, "invalid samples must not enter the WAV payload");
    require(bytes[44U] == 0x00U && bytes[45U] == 0x20U,
            "valid sample after rejected input should be encoded correctly");
    std::remove(path.c_str());
}


void test_rejects_unrepresentable_frame_count_without_reading_past_buffer() {
    const std::string path = "audio_studio_frame_count_overflow.wav";
    const std::array<float, 1U> sample{0.25F};
    {
        audio_studio::WavWriter writer;
        require(writer.open(path, {48000U, 1U}),
                "frame-count overflow writer should open");

        // Only one float is allocated. The huge request must be rejected by
        // size validation before the writer attempts to read the sample buffer.
        require(!writer.write_frames(sample.data(),
                                     std::numeric_limits<std::size_t>::max()),
                "unrepresentable frame count must be rejected before reading samples");
        require(writer.frames_written() == 0U,
                "rejected frame count must not change the written-frame count");

        require(writer.write_frames(sample.data(), sample.size()),
                "writer should remain usable after rejecting an oversized request");
        require(writer.frames_written() == 1U,
                "only the subsequent valid frame should be counted");
        require(writer.finalize(),
                "writer should safely finalize after rejecting oversized input");
    }

    require(std::filesystem::file_size(path) == 46U,
            "oversized request must not append data to the finalized WAV");
    std::remove(path.c_str());
}


void test_abort_discards_partial_output() {
    const std::string path = "audio_studio_aborted_test.wav";
    const std::array<float, 8> samples{0.1F, 0.2F, 0.3F, 0.4F, 0.5F, 0.6F, 0.7F, 0.8F};
    {
        audio_studio::WavWriter writer;
        require(writer.open(path, {48000U, 1U}), "aborted writer should open");
        require(writer.write_frames(samples.data(), samples.size()), "aborted writer should accept initial frames");
        writer.abort();
        require(!writer.is_open(), "abort should close the output stream");
    }
    require(!std::ifstream(path, std::ios::binary).good(),
            "abort must remove the incomplete WAV instead of leaving a misleading file");
    require(!has_partial_file_for(path), "abort must remove the temporary partial file too");
}

void test_preserve_partial_keeps_recoverable_source() {
    const std::string path = "audio_studio_preserved_test.wav";
    const std::array<float, 8> samples{0.1F, 0.2F, 0.3F, 0.4F, 0.5F, 0.6F, 0.7F, 0.8F};
    {
        audio_studio::WavWriter writer;
        require(writer.open(path, {48000U, 1U}), "preserved writer should open");
        require(writer.write_frames(samples.data(), samples.size()), "preserved writer should accept initial frames");
        writer.preserve_partial();
        require(!writer.is_open(), "preserve_partial should close the output stream");
    }
    require(!std::ifstream(path, std::ios::binary).good(),
            "preserving an incomplete take must not publish the final WAV path");
    require(has_partial_file_for(path),
            "preserving an incomplete take must retain its temporary partial for recovery");
    for (const auto& entry : std::filesystem::directory_iterator(".")) {
        if (entry.path().filename().string().rfind(path + ".partial-", 0U) == 0U)
            std::filesystem::remove(entry.path());
    }
}

void test_destructor_discards_unfinalized_output() {
    const std::string path = "audio_studio_unfinalized_test.wav";
    const std::array<float, 8> samples{0.1F, 0.2F, 0.3F, 0.4F, 0.5F, 0.6F, 0.7F, 0.8F};
    {
        audio_studio::WavWriter writer;
        require(writer.open(path, {48000U, 1U}), "unfinalized writer should open");
        require(writer.write_frames(samples.data(), samples.size()), "unfinalized writer should accept initial frames");
        // Intentionally leave scope without finalize(): destructor must discard this session.
    }
    require(!std::ifstream(path, std::ios::binary).good(),
            "destroying an unfinalized writer must not publish an incomplete WAV");
    require(!has_partial_file_for(path), "destruction must clean up the temporary partial file");
}

void test_repeated_long_recordings() {
    const std::vector<float> block(480U, 0.125F);
    for (int session = 0; session < 3; ++session) {
        const std::string path = "audio_studio_session_" + std::to_string(session) + ".wav";
        {
            audio_studio::WavWriter writer;
            require(writer.open(path, {48000U, 1U}), "each recording session should open cleanly");
            for (int chunk = 0; chunk < 6000; ++chunk) {
                require(writer.write_frames(block.data(), block.size()),
                        "long recording chunks should be written without errors");
            }
            require(writer.frames_written() == 2880000U,
                    "sixty seconds of mono 48 kHz audio should count 2880000 frames");
            require(writer.finalize(), "each recording session should finalize cleanly");
            require(!writer.write_frames(block.data(), block.size()),
                    "writes after finalization must be rejected");
        }

        std::ifstream file(path, std::ios::binary);
        const std::vector<unsigned char> bytes{
            std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        require(bytes.size() == 44U + 2880000U * 2U,
                "long session should have a complete PCM16 payload");
        require(read_u32(bytes, 4U) == bytes.size() - 8U,
                "long session RIFF size should match the actual file");
        require(read_u32(bytes, 40U) == 2880000U * 2U,
                "long session data size should match all written samples");
        std::remove(path.c_str());
    }
}


void test_five_minute_recording_integrity() {
    const std::string path = "audio_studio_five_minute_test.wav";
    const std::vector<float> block(480U, 0.125F); // 10 ms at 48 kHz mono.
    constexpr std::uint64_t expected_frames = 14400000U;
    {
        audio_studio::WavWriter writer;
        require(writer.open(path, {48000U, 1U}), "five-minute writer should open");
        for (int chunk = 0; chunk < 30000; ++chunk) {
            require(writer.write_frames(block.data(), block.size()),
                    "five-minute recording should accept every audio block");
        }
        require(writer.frames_written() == expected_frames,
                "five-minute recording should count every captured frame");
        require(writer.finalize(), "five-minute recording should finalize");
        require(!writer.write_frames(block.data(), block.size()),
                "five-minute finalized recording must reject further writes");
    }

    const auto actual_size = std::filesystem::file_size(path);
    require(actual_size == 44U + expected_frames * 2U,
            "five-minute WAV should contain the complete PCM16 payload");
    std::ifstream file(path, std::ios::binary);
    std::array<unsigned char, 44U> header{};
    file.read(reinterpret_cast<char*>(header.data()),
              static_cast<std::streamsize>(header.size()));
    require(static_cast<bool>(file), "five-minute WAV header should be readable");
    const std::vector<unsigned char> header_bytes(header.begin(), header.end());
    require(std::string(header.begin(), header.begin() + 4) == "RIFF",
            "five-minute WAV should retain its RIFF signature");
    require(read_u32(header_bytes, 4U) == actual_size - 8U,
            "five-minute WAV RIFF length should match the file");
    require(read_u32(header_bytes, 40U) == expected_frames * 2U,
            "five-minute WAV data length should match all frames");
    file.seekg(-2, std::ios::end);
    std::array<unsigned char, 2U> final_sample{};
    file.read(reinterpret_cast<char*>(final_sample.data()), 2);
    require(file && final_sample[0] == 0x00U && final_sample[1] == 0x10U,
            "final audio block should be intact at the end of the WAV payload");
    file.close();
    std::remove(path.c_str());
}

}
int main() {
    test_wav_header_and_samples();
    test_existing_destination_is_never_overwritten();
    test_rejects_invalid_input();
    test_rejects_unrepresentable_frame_count_without_reading_past_buffer();
    test_abort_discards_partial_output();
    test_preserve_partial_keeps_recoverable_source();
    test_destructor_discards_unfinalized_output();
    test_repeated_long_recordings();
    test_five_minute_recording_integrity();
    std::cout << "All WAV writer tests passed.\n";
    return EXIT_SUCCESS;
}
