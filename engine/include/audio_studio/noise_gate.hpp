#pragma once

#include "audio_studio/audio_types.hpp"

namespace audio_studio {

// Conservative downward expander for reducing low-level room/electronic noise
// between speech phrases. This is not a spectral denoiser or dereverberator.
struct NoiseGateSettings {
    float threshold_dbfs{-48.0F};
    float attenuation_db{-18.0F};
    float attack_ms{5.0F};
    float release_ms{120.0F};

    [[nodiscard]] bool is_valid() const noexcept;
};

class NoiseGate final {
public:
    explicit NoiseGate(NoiseGateSettings settings = {}) noexcept;

    [[nodiscard]] bool set_settings(NoiseGateSettings settings) noexcept;
    [[nodiscard]] const NoiseGateSettings& settings() const noexcept { return settings_; }

    // Processes normalized interleaved samples in place. State persists across
    // calls so adjacent blocks do not reset the envelope or cause block-edge clicks.
    [[nodiscard]] bool process(AudioBuffer& buffer) noexcept;
    void reset() noexcept;

private:
    NoiseGateSettings settings_{};
    float envelope_{0.0F};
    float gain_{1.0F};
};
