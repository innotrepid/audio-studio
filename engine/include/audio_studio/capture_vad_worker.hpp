#pragma once
#include "audio_studio/native_vad_session.hpp"
#include <cstddef>
#include <deque>
#include <memory>
#include <vector>

namespace audio_studio {

// Bounded handoff from capture to a non-real-time worker. submit_audio() only
// copies PCM; process_one()/finish() must be called by the VAD worker thread.
class CaptureVadWorker {
public:
    enum class Status { ready, backpressure, processing_failed, finished, invalid };
    CaptureVadWorker(std::unique_ptr<NativeVadSessionInterface> session,
                    std::size_t max_queued_frames);
    Status submit_audio(const float* interleaved, std::size_t frames);
    Status process_one();
    Status finish();
    bool pop_region(SpeechRegion& region);
    std::size_t queued_frames() const noexcept;
    bool failed() const noexcept;
private:
    struct Chunk { std::vector<float> pcm; std::size_t frames; };
    std::unique_ptr<NativeVadSessionInterface> session_;
    std::deque<Chunk> queue_;
    std::deque<SpeechRegion> regions_;
    std::size_t max_queued_frames_;
    std::size_t queued_frames_{0};
    bool failed_{false};
    bool finished_{false};
};
}
