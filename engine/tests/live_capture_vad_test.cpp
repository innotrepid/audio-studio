#include "audio_studio/live_capture_vad.hpp"
#include "audio_studio/wav_audio_reader.hpp"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    using namespace audio_studio;
    if (argc != 3) {
        std::cerr << "Expected paths to the pinned Silero ONNX model and speech WAV fixture.\n";
        return 1;
    }

    std::string error;
    LiveCaptureVad::Options options;
    options.ring_capacity_frames = 4096;
    options.worker_queue_frames = 4096;
    options.max_pending_regions = 64;
    options.controller_batch_frames = 512;

    auto pipeline = LiveCaptureVad::create(argv[1], 16000, 1, options, error);
    if (!pipeline || !error.empty()) {
        std::cerr << "Could not create live VAD pipeline: " << error << "\n";
        return 2;
    }

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
    SpeechRegion region{};
    while (pipeline->pop_region(region)) {
        if (region.start_ms >= region.end_ms) {
            std::cerr << "Live VAD delivered an invalid silence-test region.\n";
            return 6;
        }
    }
    if (pipeline->pending_region_count() != 0U) {
        std::cerr << "Live VAD pending count did not reach zero after draining.\n";
        return 7;
    }
    pipeline->finish();
    if (!pipeline->finished() || pipeline->failed()) {
        std::cerr << "Repeated finish() changed the completed pipeline state.\n";
        return 8;
    }

    std::vector<float> speech;
    if (!load_wav_for_transcription(argv[2], speech, &error) || speech.empty()) {
        std::cerr << "Could not decode recorded-speech VAD fixture: " << error << "\n";
        return 9;
    }
    auto speech_pipeline = LiveCaptureVad::create(argv[1], 16000, 1, options, error);
    if (!speech_pipeline) {
        std::cerr << "Could not create speech VAD pipeline: " << error << "\n";
        return 10;
    }
    constexpr std::size_t kChunkFrames = 1600U;
    for (std::size_t offset = 0; offset < speech.size(); offset += kChunkFrames) {
        const std::size_t count = std::min(kChunkFrames, speech.size() - offset);
        if (speech_pipeline->submit_audio(speech.data() + offset, count) !=
            CapturePcmRing::Status::ready) {
            std::cerr << "Speech VAD rejected a valid captured-audio chunk.\n";
            return 11;
        }
    }
    speech_pipeline->finish();
    if (!speech_pipeline->finished() || speech_pipeline->failed()) {
        std::cerr << "Speech VAD failed during final flush: "
                  << speech_pipeline->last_error() << "\n";
        return 12;
    }
    std::size_t region_count = 0U;
    std::uint64_t previous_end = 0U;
    while (speech_pipeline->pop_region(region)) {
        if (region.start_ms >= region.end_ms || region.start_ms < previous_end) {
            std::cerr << "Speech VAD returned invalid or out-of-order regions.\n";
            return 13;
        }
        previous_end = region.end_ms;
        ++region_count;
    }
    if (region_count == 0U || speech_pipeline->pending_region_count() != 0U) {
        std::cerr << "Speech VAD did not deliver final speech regions exactly once.\n";
        return 14;
    }

    options.max_pending_regions = 0U;
    auto invalid = LiveCaptureVad::create(argv[1], 16000, 1, options, error);
    if (invalid || error.empty()) {
        std::cerr << "Zero pending-region capacity must be rejected with a diagnostic.\n";
        return 15;
    }
    std::cout << "LiveCaptureVad silence, speech, flush, ordering, and drain tests passed.\n";
    return 0;
}
