#include "audio_studio/silero_onnx.hpp"
#include "audio_studio/native_api.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    using namespace audio_studio;
    if (argc != 2) {
        std::cerr << "Expected path to the pinned Silero ONNX model.\n";
        return 1;
    }

    // Validate C ABI argument handling before loading the real model.
    size_t written = 99;
    if (audio_studio_vad_push_audio(nullptr, nullptr, 0, nullptr, 0, &written) != 0 ||
        written != 0) {
        std::cerr << "A null VAD handle must fail and zero written.\n";
        return 6;
    }
    if (audio_studio_vad_flush(nullptr, nullptr, 0, &written) != 0 ||
        written != 0) {
        std::cerr << "Flushing a null VAD handle must fail and zero written.\n";
        return 7;
    }
    if (audio_studio_vad_reset(nullptr) != 0) {
        std::cerr << "Resetting a null VAD handle must fail.\n";
        return 8;
    }
    if (audio_studio_vad_create(L"invalid-threshold-test.onnx", 16000, 1,
                                std::numeric_limits<float>::quiet_NaN(), 0, 0, 0) != nullptr ||
        std::string(audio_studio_vad_last_error()).empty()) {
        std::cerr << "An invalid threshold must fail with a diagnostic.\n";
        return 9;
    }

    std::string error;
    auto missing = OnnxSileroVadStream::create(
        "missing-silero-model-for-test.onnx", error);
    if (missing || error.empty()) {
        std::cerr << "A missing model must fail with a diagnostic.\n";
        return 2;
    }

    auto stream = OnnxSileroVadStream::create(argv[1], error);
    if (!stream || !error.empty()) {
        std::cerr << "Could not initialize the pinned Silero model: " << error << "\n";
        return 3;
    }

    float probability = 0.375F;
    if (stream->process_window(nullptr, probability) || probability != 0.375F) {
        std::cerr << "Null audio must be rejected without modifying output.\n";
        return 4;
    }

    std::vector<float> samples(kSileroWindowSamples, 0.0F);
    samples[17] = std::numeric_limits<float>::quiet_NaN();
    if (stream->process_window(samples.data(), probability) || probability != 0.375F) {
        std::cerr << "Non-finite audio must be rejected without modifying output.\n";
        return 5;
    }

    stream->reset();
    stream->reset();

    // Exercise the public streaming C ABI lifecycle with the pinned model.
    AudioStudioVadHandle* handle = audio_studio_vad_create(
        std::filesystem::path(argv[1]).wstring().c_str(), 16000, 1,
        0.5F, 1, 100, 0);
    if (!handle) {
        std::cerr << "C ABI VAD creation failed: " << audio_studio_vad_last_error() << "\n";
        return 10;
    }
    std::vector<float> silent_chunk(512, 0.0F);
    written = 99;
    if (!audio_studio_vad_push_audio(handle, silent_chunk.data(), silent_chunk.size(),
                                     nullptr, 0, &written) || written != 0) {
        std::cerr << "Silent C ABI push should succeed without output: "
                  << audio_studio_vad_last_error() << "\n";
        audio_studio_vad_destroy(handle);
        return 11;
    }
    if (!audio_studio_vad_flush(handle, nullptr, 0, &written)) {
        std::cerr << "C ABI flush failed: " << audio_studio_vad_last_error() << "\n";
        audio_studio_vad_destroy(handle);
        return 12;
    }
    // Repeated flush is idempotent after all output has been delivered.
    written = 99;
    if (!audio_studio_vad_flush(handle, nullptr, 0, &written) || written != 0) {
        std::cerr << "Repeated empty flush should succeed: "
                  << audio_studio_vad_last_error() << "\n";
        audio_studio_vad_destroy(handle);
        return 13;
    }

    // Reset after flush reopens the handle for a fresh stream.
    if (!audio_studio_vad_reset(handle)) {
        std::cerr << "C ABI reset after flush failed: "
                  << audio_studio_vad_last_error() << "\n";
        audio_studio_vad_destroy(handle);
        return 14;
    }
    written = 99;
    if (!audio_studio_vad_push_audio(handle, silent_chunk.data(), silent_chunk.size(),
                                     nullptr, 0, &written) || written != 0) {
        std::cerr << "Push after reset should succeed: "
                  << audio_studio_vad_last_error() << "\n";
        audio_studio_vad_destroy(handle);
        return 15;
    }
    if (!audio_studio_vad_flush(handle, nullptr, 0, &written) || written != 0) {
        std::cerr << "Fresh stream flush after reset failed: "
                  << audio_studio_vad_last_error() << "\n";
        audio_studio_vad_destroy(handle);
        return 16;
    }
    audio_studio_vad_destroy(handle);        return 13;
    }
    audio_studio_vad_destroy(handle);

    std::cout << "Silero ONNX adapter validation tests passed.\n";
    return 0;
}
