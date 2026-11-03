#pragma once
#include <atomic>
#include <cstddef>
#include <vector>
namespace audio_studio {
// Single-producer/single-consumer preallocated PCM ring.
// submit_audio() is allocation-free and lock-free; packets are accepted whole.
class CapturePcmRing {
public:
 enum class Status { ready, backpressure, invalid, closed };
 CapturePcmRing(std::size_t capacity_frames, std::size_t channels);
 Status submit_audio(const float* interleaved, std::size_t frames) noexcept;
 std::size_t pop_audio(float* destination, std::size_t max_frames) noexcept;
 void close() noexcept;
 std::size_t queued_frames() const noexcept;
 std::size_t capacity_frames() const noexcept;
private:
 std::vector<float> samples_;
 const std::size_t capacity_frames_;
 const std::size_t channels_;
 // Keep producer and consumer counters cache-line separated without aligning
 // the containing class (MSVC /W4 emits C4324 for padded class layouts).
 struct alignas(64) Counter { std::atomic<std::size_t> value{0}; };
 Counter write_frame_;
 Counter read_frame_;
 std::atomic<bool> closed_{false};
};
}
