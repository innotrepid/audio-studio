#pragma once

#include "audio_studio/audio_types.hpp"

#include <memory>
#include <string>

namespace audio_studio {

// Optional, offline-only DSP effects. A zero gain and zero high-pass cutoff
// bypass the engine entirely, preserving the existing processing sound.
struct DspEffectsSettings {
    float gain_db{0.0F};
    // 0 disables the high-pass filter; otherwise 20..500 Hz.
    float high_pass_hz{0.0F};
    // Protects positive gain boosts with the DSP engine's limiter.
    bool limit_boost{true};

    [[nodiscard]] bool is_valid(const AudioFormat& format) const noexcept;
};

class DspEffects {
public:
    DspEffects();
    ~DspEffects();
    DspEffects(DspEffects&&) noexcept;
    DspEffects& operator=(DspEffects&&) noexcept;
    DspEffects(const DspEffects&) = delete;
    DspEffects& operator=(const DspEffects&) = delete;

    [[nodiscard]] bool configure(const AudioFormat& format,
                                 DspEffectsSettings settings,
                                 std::string* error = nullptr);
    [[nodiscard]] bool process(AudioBuffer& buffer, std::string* error = nullptr);
    [[nodiscard]] bool enabled() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace audio_studio
