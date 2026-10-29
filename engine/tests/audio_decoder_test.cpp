#include "audio_studio/audio_decoder.hpp"
#include "audio_studio/wav_writer.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
void require(bool ok, const char* message) {
    if (!ok) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void test_streaming_reads_and_seek() {
    const std::string path = "audio_studio_decoder_stream.wav";
    const std::array<float, 8U> samples{
        -1.0F, 1.0F, -0.5F, 0.5F, -0.25F, 0.25F, 0.0F, 0.75F};
    {
        audio_studio::WavWriter writer;
        require(writer.open(path, {48000U, 2U}), "create decoder fixture");
        require(writer.write_frames(samples.data(), 4U), "write decoder fixture");
        require(writer.finalize(), "finalize decoder fixture");
    }

    std::string error;
    auto decoder = audio_studio::open_audio_decoder(path, &error);
    require(decoder != nullptr, "open streaming decoder");
    require(error.empty(), "successful decoder open clears error");
    require(decoder->format().sample_rate_hz == 48000U &&
            decoder->format().channel_count == 2U, "decoder reports source format");
    require(decoder->total_frames() == 4U && decoder->position_frames() == 0U,
            "decoder reports frame count and initial position");

    std::array<float, 4U> block{};
    std::size_t frames_read = 99U;
    require(decoder->read_frames(block.data(), 2U, frames_read, &error),
            "read first bounded block");
    require(frames_read == 2U && decoder->position_frames() == 2U,
            "first read advances by frames, not samples");
    require(block[0] == -1.0F && block[1] == 1.0F &&
            block[2] < -0.49F && block[3] > 0.49F,
            "stream decoder returns normalized interleaved samples");

    require(decoder->seek_frame(1U, &error), "seek to a frame boundary");
    require(decoder->read_frames(block.data(), 2U, frames_read, &error),
            "read after seek");
    require(frames_read == 2U && block[0] < -0.49F && block[1] > 0.49F,
            "seek positions decoder at requested frame");

    require(decoder->seek_frame(4U, &error), "seek exactly to end");
    require(decoder->read_frames(block.data(), 2U, frames_read, &error),
            "read at end returns clean EOF");
    require(frames_read == 0U, "EOF is represented by zero frames");
    require(!decoder->seek_frame(5U, &error) && !error.empty(),
            "out-of-range seek is rejected with an error");
    require(!decoder->read_frames(nullptr, 1U, frames_read, &error) && !error.empty(),
            "null destination is rejected");
    std::remove(path.c_str());
}
}  // namespace

int main() {
    test_streaming_reads_and_seek();
    std::cout << "All streaming decoder tests passed.\n";
    return EXIT_SUCCESS;
}
