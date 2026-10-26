#include "audio_studio/noise_gate.hpp"

#include <algorithm>
#include <cmath>

namespace audio_studio {
namespace {
constexpr float kMinThresholdDb = -90.0F;
constexpr float kMaxThresholdDb = -6.0F;
constexpr float kMinAttenuationDb = -36.0F;
constexpr float kMaxAttenuationDb = 0.0F;
constexpr float kMinTimeMs = 0.1F;
constexpr float kMaxTimeMs = 2000.0F;
constexpr float kSilenceFloor = 1.0e-9F;

float db_to_linear(float db) noexcept {
    return std::pow(10.0F, db / 20.0F);
}

float smoothing_coefficient(float time_ms, std::uint32_t sample_rate) noexcept {
    const float time_seconds = time_ms / 1000.0F;
    return std::exp(-1.0F / (time_seconds * static_cast<float>(sample_rate)));
}
}  // namespace

bool NoiseGateSettings::is_valid() const noexcept {
    return std::isfinite(threshold_dbfs) &&
           threshold_dbfs >= kMinThresholdDb && threshold_dbfs <= kMaxThresholdDb &&
           std::isfinite(attenuation_db) &&
           attenuation_db >= kMinAttenuationDb && attenuation_db <= kMaxAttenuationDb &&
           std::isfinite(attack_ms) && attack_ms >= kMinTimeMs && attack_ms <= kMaxTimeMs &&
           std::isfinite(release_ms) && release_ms >= kMinTimeMs && release_ms <= kMaxTimeMs;
}

NoiseGate::NoiseGate(NoiseGateSettings settings) noexcept {
    if (settings.is_valid()) settings_ = settings;
}

bool NoiseGate::set_settings(NoiseGateSettings settings) noexcept {
    if (!settings.is_valid()) return false;
    settings_ = settings;
    reset();
    return true;
}

void NoiseGate::reset() noexcept {
    envelope_ = 0.0F;
    gain_ = 1.0F;
}

bool NoiseGate::process(AudioBuffer& buffer) noexcept {
    if (!buffer.is_valid()) return false;

    const float threshold = db_to_linear(settings_.threshold_dbfs);
    const float floor_gain = db_to_linear(settings_.attenuation_db);
    const float attack = smoothing_coefficient(settings_.attack_ms, buffer.format.sample_rate_hz);
    const float release = smoothing_coefficient(settings_.release_ms, buffer.format.sample_rate_hz);
    const auto channels = static_cast<std::size_t>(buffer.format.channel_count);

    for (std::size_t frame = 0; frame < buffer.frame_count(); ++frame) {
        float peak = 0.0F;
        const std::size_t offset = frame * channels;
        for (std::size_t channel = 0; channel < channels; ++channel) {
            peak = std::max(peak, std::abs(buffer.samples[offset + channel]));
        }

        const float envelope_coefficient = peak > envelope_ ? attack : release;
        envelope_ = envelope_coefficient * envelope_ +
                    (1.0F - envelope_coefficient) * peak;

        // A gentle downward expansion attenuates quieter material progressively
        // rather than hard-muting it; louder speech remains at unity gain.
        float target_gain = 1.0F;
        if (envelope_ < threshold && threshold > kSilenceFloor) {
            const float normalized = std::clamp(envelope_ / threshold, 0.0F, 1.0F);
            target_gain = floor_gain + (1.0F - floor_gain) * normalized * normalized;
        }

        const float gain_coefficient = target_gain > gain_ ? attack : release;
        gain_ = gain_coefficient * gain_ + (1.0F - gain_coefficient) * target_gain;
        for (std::size_t channel = 0; channel < channels; ++channel) {
            buffer.samples[offset + channel] *= gain_;
        }
    }
    return true;
}

}  // namespace audio_studio
