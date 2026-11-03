#include "audio_studio/capture_vad_worker.hpp"
#include <limits>
#include <utility>

namespace audio_studio {
CaptureVadWorker::CaptureVadWorker(std::unique_ptr<NativeVadSessionInterface> session,
                                   std::size_t max_queued_frames)
    : session_(std::move(session)), max_queued_frames_(max_queued_frames) {}

CaptureVadWorker::Status CaptureVadWorker::submit_audio(const float* pcm, std::size_t frames) {
    if (!session_ || channels_ == 0 || finished_ || failed_ || (frames != 0 && pcm == nullptr))
        return Status::invalid;
    if (frames > max_queued_frames_ - queued_frames_)
        return Status::backpressure;
    if (frames == 0) return Status::ready;
    // NativeVadSession owns channel count; chunks are copied unchanged and the
    // capture adapter supplies frame-aligned interleaved samples.
    Chunk chunk;
    chunk.frames = frames;
    if (frames > std::numeric_limits<std::size_t>::max() / channels_) return Status::invalid;
    chunk.pcm.assign(pcm, pcm + frames * channels_);
    queue_.push_back(std::move(chunk));
    queued_frames_ += frames;
    return Status::ready;
}

CaptureVadWorker::Status CaptureVadWorker::process_one() {
    if (!session_ || finished_ || failed_) return Status::invalid;
    if (queue_.empty()) return Status::ready;
    Chunk chunk = std::move(queue_.front());
    queue_.pop_front();
    queued_frames_ -= chunk.frames;
    std::vector<SpeechRegion> completed;
    if (!session_->push_audio(chunk.pcm.data(), chunk.frames, completed)) {
        failed_ = true;
        queue_.clear();
        queued_frames_ = 0;
        return Status::processing_failed;
    }
    for (const auto& region : completed) regions_.push_back(region);
    return Status::ready;
}

CaptureVadWorker::Status CaptureVadWorker::finish() {
    if (!session_ || failed_) return Status::invalid;
    if (!queue_.empty()) return Status::backpressure;
    if (finished_) return Status::finished;
    std::vector<SpeechRegion> completed;
    if (!session_->flush(completed)) {
        failed_ = true;
        return Status::processing_failed;
    }
    for (const auto& region : completed) regions_.push_back(region);
    finished_ = true;
    return Status::finished;
}

bool CaptureVadWorker::pop_region(SpeechRegion& region) {
    if (regions_.empty()) return false;
    region = regions_.front();
    regions_.pop_front();
    return true;
}
std::size_t CaptureVadWorker::queued_frames() const noexcept { return queued_frames_; }
bool CaptureVadWorker::failed() const noexcept { return failed_; }
}
