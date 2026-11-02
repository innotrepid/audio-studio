#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace audio_studio {
bool convert_to_mono(const float* interleaved, std::size_t frame_count,
 std::uint32_t input_sample_rate, std::uint32_t channel_count,
 std::uint32_t output_sample_rate, std::vector<float>& output) noexcept;
} // namespace audio_studio