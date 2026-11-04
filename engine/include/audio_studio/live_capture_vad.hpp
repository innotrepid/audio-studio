#pragma once

#include "audio_studio/capture_pcm_ring.hpp"
#include "audio_studio/capture_vad_controller.hpp"
#include "audio_studio/capture_vad_worker.hpp"
#include "audio_studio/native_vad_session.hpp"
#include "audio_studio/vad.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace audio_studio {

// Optional live-capture VAD path. Capture callbacks only call submit_audio()
// (allocation-free ring write). A dedicated worker drains the ring, runs Silero
// inference, and queues completed SpeechRegion values for the UI thread.
class LiveCaptureVad {
public:
    struct Options {
        VadOptions vad{};
        std::size_t ring_capacity_frames{1U << 16};
        std::size_t worker_queue_frames{1U << 16};
        std::size_t max_pending_regions{128};
        std::size_t controller_batch_frames{2048};
    };

    // Creates the ONNX session and starts the worker thread. Returns nullptr on
    // failure; error_message is set. Sample rate/channels must match capture PCM.
    static std::unique_ptr<LiveCaptureVad> create(
        const std::string& silero_model_path,
        std::uint32_t sample_rate,
        std::uint32_t channels,
        Options options,
        std::string& error_message);

    ~LiveCaptureVad();

    LiveCaptureVad(const LiveCaptureVad&) = delete;
    LiveCaptureVad& operator=(const LiveCaptureVad&) = delete;

    // Real-time safe: copies a whole packet into the SPSC ring or reports
    // backpressure. Never runs ONNX inference on the calling thread.
    CapturePcmRing::Status submit_audio(const float* interleaved,
                                        std::size_t frames) noexcept;

    // Blocks until the worker has drained the ring and flushed the VAD session.
    // Safe to call once from the recording stop path.
    void finish();

    bool failed() const noexcept;
    bool finished() const noexcept;
    std::string last_error() const;

    // UI / non-RT: drain completed regions (16 kHz mono timeline, milliseconds).
    bool pop_region(SpeechRegion& region);
    std::size_t pending_region_count() const;

private:
    LiveCaptureVad(std::unique_ptr<NativeVadSession> session,
                   std::uint32_t channels,
                   Options options);

    void worker_loop();

    Options options_;
    CapturePcmRing ring_;
    CaptureVadWorker worker_;
    CaptureVadController controller_;
    std::thread thread_;
    std::mutex wake_mutex_;
    std::condition_variable wake_cv_;
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> failed_{false};
    std::atomic<bool> finished_{false};
    mutable std::mutex error_mutex_;
    std::string error_;
    mutable std::mutex regions_mutex_;
    std::vector<SpeechRegion> completed_regions_;
    std::size_t region_read_{0};
};

}  // namespace audio_studio
