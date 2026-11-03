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
 // Atomics remain naturally aligned; avoid explicit cache-line alignment
 // because MSVC /W4 treats its padding warning as an error in this project.
 std::atomic<std::size_t> write_frame_{0};
 std::atomic<std::size_t> read_frame_{0};
 std::atomic<bool> closed_{false};
};
}
