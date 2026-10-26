#include "audio_studio/noise_gate.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\\n';
        std::exit(EXIT_FAILURE);
    }
}

float rms(const audio_studio::AudioBuffer& buffer, std::size_t start, std::size_t end) {
    double sum = 0.0;
    for (std::size_t i = start; i < end; ++i) {
        const double sample = buffer.samples[i];
        sum += sample * sample;
    }
    return end > start ? static_cast<float>(std::sqrt(sum / static_cast<double>(end - start))) : 0.0F;
}

void test_reduces_low_level_noise() {
    constexpr std::uint32_t sample_rate = 48000U;
    audio_studio::AudioBuffer buffer{audio_studio::AudioFormat{sample_rate, 1U}, {}};
    buffer.samples.resize(sample_rate, 0.003F);
    audio_studio::NoiseGate gate;
    require(gate.process(buffer), "valid mono audio should process");
    require(rms(buffer, 12000U, 24000U) < 0.0015F,
            "steady low-level noise should be attenuated after the envelope settles");
}

void test_preserves_louder_speech_and_block_continuity() {
    constexpr std::uint32_t sample_rate = 48000U;
    audio_studio::AudioBuffer first{audio_studio::AudioFormat{sample_rate, 1U}, {}};
    first.samples.resize(12000U, 0.002F);
    audio_studio::AudioBuffer speech{audio_studio::AudioFormat{sample_rate, 1U}, {}};
    speech.samples.resize(12000U);
    for (std::size_t i = 0; i < speech.samples.size(); ++i) {
        speech.samples[i] = 0.2F * std::sin(2.0F * 3.14159265358979323846F * 220.0F *
                                            static_cast<float>(i) / sample_rate);
    }
    audio_studio::NoiseGate gate;
    require(gate.process(first), "first block should process");
    require(gate.process(speech), "speech block should process");
    require(rms(speech, 3000U, 11000U) > 0.12F,
            "speech-level signal should remain substantially intact");
    require(std::isfinite(speech.samples.back()), "output samples must remain finite");
}

void test_rejects_invalid_settings_and_audio() {
    audio_studio::NoiseGate gate;
    require(!gate.set_settings({std::numeric_limits<float>::quiet_NaN(), -18.0F, 5.0F, 120.0F}),
            "NaN threshold should be rejected");
    require(!gate.set_settings({-48.0F, -50.0F, 5.0F, 120.0F}),
            "excessive attenuation should be rejected");
    audio_studio::AudioBuffer invalid{audio_studio::AudioFormat{48000U, 1U},
                                      {std::numeric_limits<float>::infinity()}};
    require(!gate.process(invalid), "non-finite audio should be rejected");
}
}  // namespace

int main() {
    test_reduces_low_level_noise();
    test_preserves_louder_speech_and_block_continuity();
    test_rejects_invalid_settings_and_audio();
    std::cout << "All noise gate tests passed.\\n";
    return EXIT_SUCCESS;
}
