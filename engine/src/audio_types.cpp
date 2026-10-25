#include "audio_studio/audio_types.hpp"

#include <cmath>

namespace audio_studio {

bool AudioFormat::is_valid() const noexcept {
    return sample_rate_hz >= 8000U && sample_rate_hz <= 192000U &&
           (channel_count == 1U || channel_count == 2U);
}

std::size_t AudioBuffer::frame_count() const noexcept {
    if (format.channel_count == 0U ||
        samples.size() % format.channel_count != 0U) {
        return 0U;
    }
    return samples.size() / format.channel_count;
}

bool AudioBuffer::is_valid() const noexcept {
    if (!format.is_valid() || samples.empty() ||
        samples.size() % format.channel_count != 0U) {
        return false;
    }
    for (const float sample : samples) {
        if (!std::isfinite(sample)) {
            return false;
        }
    }
    return true;
}

}  // namespace audio_studio
