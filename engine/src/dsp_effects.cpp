#include "audio_studio/dsp_effects.hpp"

#include "dsp_engine.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace audio_studio {
namespace {
constexpr float kMinGainDb = -24.0F;
constexpr float kMaxGainDb = 12.0F;
constexpr float kMinHighPassHz = 20.0F;
constexpr float kMaxHighPassHz = 500.0F;
constexpr int kFramesPerBlock = 4096;

bool fail(std::string* error, const char* message) {
    if (error != nullptr) *error = message;
    return false;
}
}  // namespace

bool DspEffectsSettings::is_valid(const AudioFormat& format) const noexcept {
    if (!format.is_valid() || !std::isfinite(gain_db) ||
        gain_db < kMinGainDb || gain_db > kMaxGainDb ||
        !std::isfinite(high_pass_hz)) {
        return false;
    }
    if (high_pass_hz == 0.0F) return true;
    return high_pass_hz >= kMinHighPassHz &&
           high_pass_hz <= kMaxHighPassHz &&
           high_pass_hz < static_cast<float>(format.sample_rate_hz) * 0.45F;
}

struct DspEffects::Impl {
    DspEngine* engine{nullptr};
    AudioFormat format{};
    DspEffectsSettings settings{};

    ~Impl() {
        if (engine != nullptr) dsp_destroy(engine);
    }
};

DspEffects::DspEffects() : impl_(std::make_unique<Impl>()) {}
DspEffects::~DspEffects() = default;
DspEffects::DspEffects(DspEffects&&) noexcept = default;
DspEffects& DspEffects::operator=(DspEffects&&) noexcept = default;

bool DspEffects::configure(const AudioFormat& format,
                           DspEffectsSettings settings,
                           std::string* error) {
    if (error != nullptr) error->clear();
    if (!settings.is_valid(format)) {
        return fail(error, "DSP effect settings or audio format are invalid.");
    }

    if (impl_->engine != nullptr) {
        dsp_destroy(impl_->engine);
        impl_->engine = nullptr;
    }
    impl_->format = format;
    impl_->settings = settings;

    if (settings.gain_db == 0.0F && settings.high_pass_hz == 0.0F) return true;

    DspConfig config{};
    config.sample_rate = static_cast<float>(format.sample_rate_hz);
    config.channels = static_cast<int>(format.channel_count);
    config.max_block = kFramesPerBlock;
    config.enable_eq = settings.high_pass_hz > 0.0F ? 1 : 0;
    config.enable_dvc = settings.gain_db != 0.0F ? 1 : 0;
    config.enable_limiter = settings.gain_db > 0.0F && settings.limit_boost ? 1 : 0;
    config.enable_dither = 0;
    config.dither_mode = DSP_DITHER_NONE;
    config.limiter_threshold_db = -1.0F;
    config.limiter_release_ms = 50.0F;

    impl_->engine = dsp_create(&config);
    if (impl_->engine == nullptr) {
        return fail(error, "DSP engine could not be initialized for this WAV format.");
    }

    if (settings.high_pass_hz > 0.0F) {
        DspEqBand high_pass{};
        high_pass.freq_hz = settings.high_pass_hz;
        high_pass.gain_db = 0.0F;
        high_pass.q = 0.70710678F;
        high_pass.type = 4;  // DSP EQ high-pass
        if (dsp_eq_set_bands(impl_->engine, &high_pass, 1) != DSP_OK ||
            dsp_eq_enable(impl_->engine, 1) != DSP_OK) {
            dsp_destroy(impl_->engine);
            impl_->engine = nullptr;
            return fail(error, "DSP engine rejected the high-pass filter settings.");
        }
    }

    if (settings.gain_db != 0.0F &&
        (dsp_dvc_set_gain_db(impl_->engine, settings.gain_db) != DSP_OK ||
         dsp_dvc_enable(impl_->engine, 1) != DSP_OK)) {
        dsp_destroy(impl_->engine);
        impl_->engine = nullptr;
        return fail(error, "DSP engine rejected the gain setting.");
    }
    return true;
}

bool DspEffects::process(AudioBuffer& buffer, std::string* error) {
    if (error != nullptr) error->clear();
    if (!buffer.is_valid() || buffer.format.sample_rate_hz != impl_->format.sample_rate_hz ||
        buffer.format.channel_count != impl_->format.channel_count) {
        return fail(error, "Audio buffer format does not match the configured DSP format.");
    }
    if (impl_->engine == nullptr) return true;

    const std::size_t channels = impl_->format.channel_count;
    const std::size_t total_frames = buffer.frame_count();
    std::vector<float> output(std::min<std::size_t>(kFramesPerBlock, total_frames) * channels);
    for (std::size_t offset = 0; offset < total_frames;) {
        const int frames = static_cast<int>(std::min<std::size_t>(
            kFramesPerBlock, total_frames - offset));
        const std::size_t sample_offset = offset * channels;
        if (dsp_process(impl_->engine, buffer.samples.data() + sample_offset,
                        output.data(), frames) != DSP_OK) {
            return fail(error, "DSP engine failed to process an audio block.");
        }
        std::copy_n(output.data(), static_cast<std::size_t>(frames) * channels,
                    buffer.samples.data() + sample_offset);
        for (std::size_t i = 0; i < static_cast<std::size_t>(frames) * channels; ++i) {
            if (!std::isfinite(output[i])) {
                return fail(error, "DSP engine produced a non-finite sample.");
            }
        }
        offset += static_cast<std::size_t>(frames);
    }
    return true;
}

bool DspEffects::enabled() const noexcept {
    return impl_ != nullptr && impl_->engine != nullptr;
}

}  // namespace audio_studio
