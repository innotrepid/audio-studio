#pragma once
#include "audio_studio/capture_pcm_ring.hpp"
#include "audio_studio/capture_vad_worker.hpp"
#include <cstddef>
#include <vector>

namespace audio_studio {

// Non-real-time consumer-side bridge. Capture only writes to CapturePcmRing;
// this controller drains it, queues frames for VAD, and runs inference here.
class CaptureVadController {
public:
    enum class Status { ready, backpressure, processing_failed, finished, invalid };
    CaptureVadController(CapturePcmRing& ring, CaptureVadWorker& worker,
                         std::size_t channels, std::size_t max_batch_frames);
    Status process_available();
    Status finish();
    bool failed() const noexcept;
private:
    CapturePcmRing& ring_;
    CaptureVadWorker& worker_;
    std::size_t channels_;
    std::vector<float> scratch_;
    bool failed_{false};
    bool finished_{false};
};
}
