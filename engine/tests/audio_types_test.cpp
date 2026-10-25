#include "audio_studio/audio_types.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void test_valid_formats() {
    require(audio_studio::AudioFormat{48000U, 1U}.is_valid(),
            "48 kHz mono format should be valid");
    require(audio_studio::AudioFormat{44100U, 2U}.is_valid(),
            "44.1 kHz stereo format should be valid");
}

void test_invalid_formats() {
    require(!audio_studio::AudioFormat{7999U, 1U}.is_valid(),
            "sample rate below supported range should be rejected");
    require(!audio_studio::AudioFormat{192001U, 1U}.is_valid(),
            "sample rate above supported range should be rejected");
    require(!audio_studio::AudioFormat{48000U, 0U}.is_valid(),
            "zero channels should be rejected");
    require(!audio_studio::AudioFormat{48000U, 3U}.is_valid(),
            "unsupported channel count should be rejected");
}

void test_interleaved_frame_count() {
    audio_studio::AudioBuffer buffer{
        audio_studio::AudioFormat{48000U, 2U}, {0.0F, 0.1F, -0.1F, 0.2F}};
    require(buffer.is_valid(), "well-formed stereo buffer should be valid");
    require(buffer.frame_count() == 2U,
            "four stereo samples should represent two frames");
}

void test_malformed_and_non_finite_buffers() {
    audio_studio::AudioBuffer odd_stereo_buffer{
        audio_studio::AudioFormat{48000U, 2U}, {0.0F, 0.1F, 0.2F}};
    require(!odd_stereo_buffer.is_valid(),
            "stereo buffer with incomplete frame should be rejected");
    require(odd_stereo_buffer.frame_count() == 0U,
            "malformed buffer should report zero frames");

    audio_studio::AudioBuffer nan_buffer{
        audio_studio::AudioFormat{48000U, 1U},
        {std::numeric_limits<float>::quiet_NaN()}};
    require(!nan_buffer.is_valid(), "NaN sample should be rejected");

    audio_studio::AudioBuffer infinite_buffer{
        audio_studio::AudioFormat{48000U, 1U},
        {std::numeric_limits<float>::infinity()}};
    require(!infinite_buffer.is_valid(), "infinite sample should be rejected");

    audio_studio::AudioBuffer empty_buffer{
        audio_studio::AudioFormat{48000U, 1U}, {}};
    require(!empty_buffer.is_valid(), "empty audio buffer should be rejected");
}

}  // namespace

int main() {
    test_valid_formats();
    test_invalid_formats();
    test_interleaved_frame_count();
    test_malformed_and_non_finite_buffers();
    std::cout << "All Audio Studio core tests passed.\n";
    return EXIT_SUCCESS;
}
