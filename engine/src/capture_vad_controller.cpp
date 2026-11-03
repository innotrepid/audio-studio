#include "audio_studio/capture_vad_controller.hpp"
#include <limits>

namespace audio_studio {
CaptureVadController::CaptureVadController(CapturePcmRing& ring, CaptureVadWorker& worker,
                                           std::size_t channels, std::size_t max_batch_frames)
    : ring_(ring), worker_(worker), channels_(channels),
      scratch_(channels != 0 && max_batch_frames <= std::numeric_limits<std::size_t>::max() / channels
                   ? max_batch_frames * channels : 0) {}

CaptureVadController::Status CaptureVadController::process_available() {
    if (failed_ || finished_ || channels_ == 0 || scratch_.empty()) return Status::invalid;
    const auto batch_frames = scratch_.size() / channels_;
    for (;;) {
        const auto frames = ring_.pop_audio(scratch_.data(), batch_frames);
        if (frames == 0) return Status::ready;
        const auto submitted = worker_.submit_audio(scratch_.data(), frames);
        if (submitted != CaptureVadWorker::Status::ready) {
            failed_ = true;
            return submitted == CaptureVadWorker::Status::backpressure
                       ? Status::backpressure : Status::processing_failed;
        }
        const auto processed = worker_.process_one();
        if (processed != CaptureVadWorker::Status::ready) {
            failed_ = true;
            return Status::processing_failed;
        }
    }
}

CaptureVadController::Status CaptureVadController::finish() {
    if (failed_) return Status::processing_failed;
    if (finished_) return Status::finished;
    ring_.close();
    const auto drained = process_available();
    if (drained != Status::ready) return drained;
    const auto flushed = worker_.finish();
    if (flushed == CaptureVadWorker::Status::finished) {
        finished_ = true;
        return Status::finished;
    }
    failed_ = true;
    return flushed == CaptureVadWorker::Status::backpressure
               ? Status::backpressure : Status::processing_failed;
}
bool CaptureVadController::failed() const noexcept { return failed_; }
}
