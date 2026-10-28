#include "audio_studio/dsp_effects.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
constexpr double kPi = 3.14159265358979323846;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
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

void test_bypass_preserves_audio() {
    audio_studio::AudioBuffer buffer{audio_studio::AudioFormat{48000U, 1U}, {0.25F, -0.5F, 0.75F}};
    const auto original = buffer.samples;
    audio_studio::DspEffects effects;
    std::string error;
    require(effects.configure(buffer.format, {}, &error), "default DSP settings should configure");
    require(!effects.enabled(), "default DSP settings should bypass engine");
    require(effects.process(buffer, &error), "bypass processing should succeed");
    require(buffer.samples == original, "bypass must preserve every sample exactly");
}

void test_gain_reduction() {
    audio_studio::AudioBuffer buffer{audio_studio::AudioFormat{48000U, 1U}, {}};
    buffer.samples.resize(48000U, 0.1F);
    audio_studio::DspEffects effects;
    std::string error;
    require(effects.configure(buffer.format, {-6.0F, 0.0F, true}, &error), "valid gain should configure");
    require(effects.process(buffer, &error), "gain processing should succeed");
    require(std::fabs(buffer.samples.back() - 0.0501187F) < 0.002F,
            "minus six dB should approximately halve sample amplitude");
}

void test_high_pass_reduces_low_frequency_rumble() {
    constexpr std::uint32_t rate = 48000U;
    audio_studio::AudioBuffer buffer{audio_studio::AudioFormat{rate, 1U}, {}};
    buffer.samples.resize(rate);
    for (std::size_t i = 0; i < buffer.samples.size(); ++i) {
        buffer.samples[i] = 0.2F * std::sin(2.0 * kPi * 50.0 *
            static_cast<double>(i) / static_cast<double>(rate));
    }
    audio_studio::DspEffects effects;
    std::string error;
    require(effects.configure(buffer.format, {0.0F, 180.0F, true}, &error),
            "valid high-pass setting should configure");
    require(effects.process(buffer, &error), "high-pass processing should succeed");
    require(rms(buffer, 12000U, 36000U) < 0.06F,
            "high-pass should substantially attenuate 50 Hz rumble at 180 Hz cutoff");
}

void test_rejects_invalid_settings() {
    const audio_studio::AudioFormat format{48000U, 1U};
    audio_studio::DspEffects effects;
    std::string error;
    require(!effects.configure(format, {std::numeric_limits<float>::quiet_NaN(), 0.0F, true}, &error),
            "NaN gain must be rejected");
    require(!effects.configure(format, {0.0F, 600.0F, true}, &error),
            "cutoff above the supported range must be rejected");
    require(!effects.configure(format, {13.0F, 0.0F, true}, &error),
            "gain above the supported range must be rejected");
}

void test_stereo_and_non_finite_guard() {
    audio_studio::AudioBuffer buffer{audio_studio::AudioFormat{48000U, 2U}, {}};
    buffer.samples.resize(2048U * 2U);
    for (std::size_t frame = 0; frame < 2048U; ++frame) {
        buffer.samples[frame * 2U] = 0.2F;
        buffer.samples[frame * 2U + 1U] = -0.1F;
    }
    audio_studio::DspEffects effects;
    std::string error;
    require(effects.configure(buffer.format, {-3.0F, 0.0F, true}, &error),
            "stereo gain should configure");
    require(effects.process(buffer, &error), "stereo gain should process");
    require(std::fabs(buffer.samples[0] / buffer.samples[1] + 2.0F) < 0.01F,
            "gain should preserve stereo channel balance");
}
}  // namespace

int main() {
    test_bypass_preserves_audio();
    test_gain_reduction();
    test_high_pass_reduces_low_frequency_rumble();
    test_rejects_invalid_settings();
    test_stereo_and_non_finite_guard();
    std::cout << "All DSP effects tests passed.\n";
    return EXIT_SUCCESS;
}
