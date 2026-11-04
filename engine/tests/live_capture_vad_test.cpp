#include "audio_studio/live_capture_vad.hpp"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    using namespace audio_studio;
    if (argc != 2) {
        std::cerr << "Expected path to the pinned Silero ONNX model.\n";
        return 1;
    }

    std::string error;
    LiveCaptureVad::Options options;
    options.ring_capacity_frames = 4096;
    options.worker_queue_frames = 4096;
    options.max_pending_regions = 8;
    options.controller_batch_frames = 512;

    auto pipeline = LiveCaptureVad::create(argv[1], 16000, 1, options, error);
    if (!pipeline || !error.empty()) {
        std::cerr << "Could not create live VAD pipeline: " << error << "\n";
        return 2;
    }

    // Submit more than one controller batch so finish() must drain queued PCM
    // before flushing the recurrent VAD session.
    std::vector<float> silence(3072, 0.0F);
    if (pipeline->submit_audio(silence.data(), silence.size()) !=
        CapturePcmRing::Status::ready) {
        std::cerr << "Live VAD rejected valid PCM input.\n";
        return 3;
    }

    pipeline->finish();
    if (!pipeline->finished() || pipeline->failed()) {
        std::cerr << "Live VAD did not finish cleanly: " << pipeline->last_error() << "\n";
        return 4;
    }
    if (pipeline->submit_audio(silence.data(), 1) != CapturePcmRing::Status::closed) {
        std::cerr << "Live VAD accepted audio after finish().\n";
        return 5;
    }

    // Draining is safe even when silence produced no regions; any delivered
    // regions must be valid and the reported queue size must reach zero.
    SpeechRegion region{};
    while (pipeline->pop_region(region)) {
        if (region.start_ms >= region.end_ms) {
            std::cerr << "Live VAD delivered an invalid speech region.\n";
            return 6;
        }
    }
    if (pipeline->pending_region_count() != 0U) {
        std::cerr << "Live VAD pending count did not reach zero after draining.\n";
        return 7;
    }

    // finish() is idempotent after the worker has completed.
    pipeline->finish();
    if (!pipeline->finished() || pipeline->failed()) {
        std::cerr << "Repeated finish() changed the completed pipeline state.\n";
        return 8;
    }

    // Validate configuration diagnostics without starting a worker.
    options.max_pending_regions = 0;
    auto invalid = LiveCaptureVad::create(argv[1], 16000, 1, options, error);
    if (invalid || error.empty()) {
        std::cerr << "Zero pending-region capacity must be rejected with a diagnostic.\n";
        return 9;
    }

    std::cout << "LiveCaptureVad lifecycle and drain tests passed.\n";
    return 0;
}
