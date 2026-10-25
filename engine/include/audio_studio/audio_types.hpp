#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace audio_studio {

struct AudioFormat {
    std::uint32_t sample_rate_hz{48000};
    std::uint16_t channel_count{1};

    [[nodiscard]] bool is_valid() const noexcept;
};

struct AudioBuffer {
    AudioFormat format{};
    // Interleaved normalized float samples. DSP may exceed [-1, 1] internally;
    // clipping is handled explicitly by a later output/encoding stage.
    std::vector<float> samples{};

    [[nodiscard]] std::size_t frame_count() const noexcept;
    [[nodiscard]] bool is_valid() const noexcept;
};

}  // namespace audio_studio
