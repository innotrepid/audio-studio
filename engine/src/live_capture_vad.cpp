#include "audio_studio/live_capture_vad.hpp"

#include <chrono>
#include <utility>

namespace audio_studio {

std::unique_ptr<LiveCaptureVad> LiveCaptureVad::create(
    const std::string& silero_model_path,
    std::uint32_t sample_rate,
    std::uint32_t channels,
    Options options,
    std::string& error_message) {
    error_message.clear();
    if (silero_model_path.empty() || sample_rate < 8000U || sample_rate > 192000U ||
        channels == 0U || channels > 32U) {
        error_message = "Invalid live-capture VAD configuration.";
        return nullptr;
    }
    if (options.ring_capacity_frames == 0U || options.worker_queue_frames == 0U ||
        options.controller_batch_frames == 0U || options.max_pending_regions == 0U) {
        error_message = "Live-capture VAD buffer sizes must be positive.";
        return nullptr;
    }

    auto session = NativeVadSession::create(
        silero_model_path, sample_rate, channels, options.vad, error_message);
    if (!session) return nullptr;

    try {
        return std::unique_ptr<LiveCaptureVad>(new LiveCaptureVad(
            std::move(session), channels, options));
    } catch (const std::exception& ex) {
        error_message = ex.what();
    } catch (...) {
        error_message = "Could not allocate live-capture VAD pipeline.";
    }
    return nullptr;
}

LiveCaptureVad::LiveCaptureVad(std::unique_ptr<NativeVadSession> session,
                               std::uint32_t channels,
                               Options options)
    : options_(options),
      ring_(options.ring_capacity_frames, channels),
      worker_(std::move(session), options.worker_queue_frames, channels,
              options.max_pending_regions),
      controller_(ring_, worker_, channels, options.controller_batch_frames) {
    thread_ = std::thread(&LiveCaptureVad::worker_loop, this);
}

LiveCaptureVad::~LiveCaptureVad() {
    finish();
    if (thread_.joinable()) thread_.join();
}

CapturePcmRing::Status LiveCaptureVad::submit_audio(const float* interleaved,
                                                    std::size_t frames) noexcept {
    if (failed_.load(std::memory_order_acquire) ||
        finished_.load(std::memory_order_acquire)) {
        return CapturePcmRing::Status::closed;
    }
    const auto status = ring_.submit_audio(interleaved, frames);
    if (status == CapturePcmRing::Status::ready) {
        wake_cv_.notify_one();
    }
    return status;
}

void LiveCaptureVad::finish() {
    if (finished_.load(std::memory_order_acquire)) return;
    stop_requested_.store(true, std::memory_order_release);
    wake_cv_.notify_one();
    if (thread_.joinable()) thread_.join();
}

bool LiveCaptureVad::failed() const noexcept {
    return failed_.load(std::memory_order_acquire);
}

bool LiveCaptureVad::finished() const noexcept {
    return finished_.load(std::memory_order_acquire);
}

std::string LiveCaptureVad::last_error() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return error_;
}

bool LiveCaptureVad::pop_region(SpeechRegion& region) {
    std::lock_guard<std::mutex> lock(regions_mutex_);
    if (region_read_ >= completed_regions_.size()) return false;
    region = completed_regions_[region_read_++];
    return true;
}

std::size_t LiveCaptureVad::pending_region_count() const {
    std::lock_guard<std::mutex> lock(regions_mutex_);
    if (region_read_ >= completed_regions_.size()) return 0;
    return completed_regions_.size() - region_read_;
}

void LiveCaptureVad::worker_loop() {
    auto mark_failed = [this](const char* message) {
        failed_.store(true, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lock(error_mutex_);
            error_ = message ? message : "Live-capture VAD failed.";
        }
    };

    for (;;) {
        {
            std::unique_lock<std::mutex> lock(wake_mutex_);
            wake_cv_.wait_for(lock, std::chrono::milliseconds(20), [this] {
                return stop_requested_.load(std::memory_order_acquire) ||
                       ring_.queued_frames() > 0U ||
                       failed_.load(std::memory_order_acquire);
            });
        }

        if (failed_.load(std::memory_order_acquire)) break;

        const auto status = controller_.process_available();
        if (status == CaptureVadController::Status::backpressure ||
            status == CaptureVadController::Status::processing_failed ||
            status == CaptureVadController::Status::invalid) {
            mark_failed("Live-capture VAD processing failed or overloaded.");
            break;
        }

        SpeechRegion region{};
        while (worker_.pop_region(region)) {
            std::lock_guard<std::mutex> lock(regions_mutex_);
            if (completed_regions_.size() >= options_.max_pending_regions) {
                mark_failed("Live-capture VAD region queue overflowed.");
                break;
            }
            completed_regions_.push_back(region);
        }
        if (failed_.load(std::memory_order_acquire)) break;

        if (stop_requested_.load(std::memory_order_acquire)) {
            const auto finished = controller_.finish();
            if (finished != CaptureVadController::Status::finished) {
                mark_failed("Live-capture VAD final flush failed.");
                break;
            }
            SpeechRegion tail{};
            while (worker_.pop_region(tail)) {
                std::lock_guard<std::mutex> lock(regions_mutex_);
                completed_regions_.push_back(tail);
            }
            finished_.store(true, std::memory_order_release);
            break;
        }
    }

    if (!finished_.load(std::memory_order_acquire)) {
        finished_.store(true, std::memory_order_release);
    }
}

}  // namespace audio_studio
