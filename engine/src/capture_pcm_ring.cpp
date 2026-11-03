#include "audio_studio/capture_pcm_ring.hpp"
#include <algorithm>
#include <limits>

namespace audio_studio {
CapturePcmRing::CapturePcmRing(std::size_t capacity_frames, std::size_t channels)
    : samples_(capacity_frames <= std::numeric_limits<std::size_t>::max() / (channels == 0 ? 1 : channels)
          ? capacity_frames * channels : 0),
      capacity_frames_(capacity_frames), channels_(channels) {}

CapturePcmRing::Status CapturePcmRing::submit_audio(const float* input, std::size_t frames) noexcept {
    if (channels_ == 0 || capacity_frames_ == 0 || (frames != 0 && input == nullptr))
        return Status::invalid;
    if (closed_.load(std::memory_order_acquire)) return Status::closed;
    if (frames == 0) return Status::ready;
    if (frames > capacity_frames_) return Status::backpressure;
    const auto write = write_frame_.load(std::memory_order_relaxed);
    const auto read = read_frame_.load(std::memory_order_acquire);
    const auto queued = write - read;
    if (queued > capacity_frames_ || frames > capacity_frames_ - queued)
        return Status::backpressure;
    const auto first_frames = (std::min)(frames, capacity_frames_ - (write % capacity_frames_));
    const auto first_samples = first_frames * channels_;
    std::copy_n(input, first_samples, samples_.data() + (write % capacity_frames_) * channels_);
    const auto remaining_frames = frames - first_frames;
    if (remaining_frames != 0)
        std::copy_n(input + first_samples, remaining_frames * channels_, samples_.data());
    write_frame_.store(write + frames, std::memory_order_release);
    return Status::ready;
}

std::size_t CapturePcmRing::pop_audio(float* output, std::size_t max_frames) noexcept {
    if (channels_ == 0 || capacity_frames_ == 0 || output == nullptr || max_frames == 0)
        return 0;
    const auto read = read_frame_.load(std::memory_order_relaxed);
    const auto write = write_frame_.load(std::memory_order_acquire);
    const auto available = write - read;
    const auto frames = (std::min)(available, max_frames);
    const auto first_frames = (std::min)(frames, capacity_frames_ - (read % capacity_frames_));
    const auto first_samples = first_frames * channels_;
    std::copy_n(samples_.data() + (read % capacity_frames_) * channels_, first_samples, output);
    const auto remaining_frames = frames - first_frames;
    if (remaining_frames != 0)
        std::copy_n(samples_.data(), remaining_frames * channels_, output + first_samples);
    read_frame_.store(read + frames, std::memory_order_release);
    return frames;
}
void CapturePcmRing::close() noexcept { closed_.store(true, std::memory_order_release); }
std::size_t CapturePcmRing::queued_frames() const noexcept {
    return write_frame_.load(std::memory_order_acquire) - read_frame_.load(std::memory_order_acquire);
}
std::size_t CapturePcmRing::capacity_frames() const noexcept { return capacity_frames_; }
}
