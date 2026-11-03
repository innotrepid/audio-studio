#include "audio_studio/silero_onnx.hpp"
#include "audio_studio/native_api.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    using namespace audio_studio;
    if (argc != 3) {
        std::cerr << "Expected paths to the pinned Silero ONNX model and WAV fixture.\n";
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

    // Real-model inference smoke test: silence should remain low-confidence,
    // while a deterministic voiced harmonic signal should produce speech-like
    // confidence. Keep one stream per sequence so recurrent state is explicit.
    std::vector<float> silence(kSileroWindowSamples, 0.0F);
    float silence_probability = 1.0F;
    for (int i = 0; i < 20; ++i) {
        if (!stream->process_window(silence.data(), silence_probability) ||
            silence_probability > 0.1F) {
            std::cerr << "Silero should classify sustained digital silence as low probability.\\n";
            return 17;
        }
    }
    stream->reset();
    float peak_voiced_probability = 0.0F;
    std::vector<float> voiced(kSileroWindowSamples);
    constexpr double pi = 3.14159265358979323846;
    for (int window = 0; window < 60; ++window) {
        for (std::size_t i = 0; i < voiced.size(); ++i) {
            const double t = static_cast<double>(window * voiced.size() + i) / 16000.0;
            const double syllabic = 0.35 + 0.65 * (0.5 + 0.5 * std::sin(2.0 * pi * 3.2 * t));
            const double carrier = std::sin(2.0 * pi * 125.0 * t) +
                0.45 * std::sin(2.0 * pi * 250.0 * t) +
                0.2 * std::sin(2.0 * pi * 375.0 * t) +
                0.1 * std::sin(2.0 * pi * 500.0 * t);
            voiced[i] = static_cast<float>(0.22 * syllabic * carrier);
        }
        float p = 0.0F;
        if (!stream->process_window(voiced.data(), p)) {
            std::cerr << "Silero rejected valid voiced test audio.\\n";
            return 18;
        }
        peak_voiced_probability = std::max(peak_voiced_probability, p);
    }
    if (peak_voiced_probability < 0.5F) {
        std::cerr << "Silero did not reach speech confidence on voiced test signal: "
                  << peak_voiced_probability << "\\n";
        return 19;
    }
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
    audio_studio_vad_destroy(handle);

    // Load the pinned upstream fixture (RIFF/WAVE PCM16 mono 16 kHz).
    std::ifstream wav(argv[2], std::ios::binary);
    auto read_u16 = [](std::istream& input, uint16_t& value) {
        unsigned char b[2]{};
        if (!input.read(reinterpret_cast<char*>(b), 2)) return false;
        value = static_cast<uint16_t>(b[0] | (static_cast<uint16_t>(b[1]) << 8));
        return true;
    };
    auto read_u32 = [](std::istream& input, uint32_t& value) {
        unsigned char b[4]{};
        if (!input.read(reinterpret_cast<char*>(b), 4)) return false;
        value = static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) |
            (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24);
        return true;
    };
    char riff[4]{}, wave[4]{};
    uint32_t riff_size = 0;
    if (!wav || !wav.read(riff, 4) || !read_u32(wav, riff_size) ||
        !wav.read(wave, 4) || std::string(riff, 4) != "RIFF" ||
        std::string(wave, 4) != "WAVE") {
        std::cerr << "Pinned fixture is not a valid RIFF/WAVE file.\n";
        return 20;
    }
    uint16_t format = 0, channels = 0, bits = 0, block_align = 0;
    uint32_t sample_rate = 0;
    std::vector<unsigned char> pcm;
    while (wav && !wav.eof()) {
        char chunk_id[4]{};
        uint32_t chunk_size = 0;
        if (!wav.read(chunk_id, 4) || !read_u32(wav, chunk_size)) break;
        const auto chunk_start = wav.tellg();
        if (std::string(chunk_id, 4) == "fmt ") {
            uint32_t byte_rate = 0;
            if (!read_u16(wav, format) || !read_u16(wav, channels) ||
                !read_u32(wav, sample_rate) || !read_u32(wav, byte_rate) ||
                !read_u16(wav, block_align) || !read_u16(wav, bits)) {
                std::cerr << "Truncated WAV format chunk.\n";
                return 21;
            }
        } else if (std::string(chunk_id, 4) == "data") {
            pcm.resize(chunk_size);
            if (!wav.read(reinterpret_cast<char*>(pcm.data()), static_cast<std::streamsize>(pcm.size()))) {
                std::cerr << "Truncated WAV data chunk.\n";
                return 22;
            }
            break;
        }
        wav.seekg(chunk_start + static_cast<std::streamoff>(chunk_size + (chunk_size & 1U)));
    }
    if (format != 1 || channels != 1 || sample_rate != 16000 ||
        bits != 16 || block_align != 2 || pcm.empty() || pcm.size() % 2 != 0) {
        std::cerr << "Fixture must be mono 16 kHz PCM16; got format=" << format
                  << ", channels=" << channels << ", rate=" << sample_rate
                  << ", bits=" << bits << ".\n";
        return 23;
    }
    std::vector<float> recorded(pcm.size() / 2);
    for (size_t i = 0; i < recorded.size(); ++i) {
        const uint16_t raw = static_cast<uint16_t>(pcm[2 * i]) |
            (static_cast<uint16_t>(pcm[2 * i + 1]) << 8);
        recorded[i] = static_cast<float>(static_cast<int16_t>(raw) / 32768.0);
    }

    // Feed irregular chunk boundaries to exercise streaming state across pushes.
    handle = audio_studio_vad_create(std::filesystem::path(argv[1]).wstring().c_str(),
        16000, 1, 0.5F, 100, 100, 30);
    if (!handle) {
        std::cerr << "Could not create VAD for recorded fixture: "
                  << audio_studio_vad_last_error() << "\n";
        return 24;
    }
    std::vector<AudioStudioSpeechRegion> regions;
    const size_t chunk_pattern[] = {137, 509, 1024, 73, 2048, 311};
    size_t offset = 0, chunk_index = 0;
    while (offset < recorded.size()) {
        const size_t count = std::min(chunk_pattern[chunk_index++ % 6], recorded.size() - offset);
        AudioStudioSpeechRegion output[128]{};
        written = 0;
        if (!audio_studio_vad_push_audio(handle, recorded.data() + offset, count,
                output, 128, &written)) {
            std::cerr << "Recorded-speech push failed: " << audio_studio_vad_last_error() << "\n";
            audio_studio_vad_destroy(handle);
            return 25;
        }
        regions.insert(regions.end(), output, output + written);
        offset += count;
    }
    AudioStudioSpeechRegion final_regions[128]{};
    written = 0;
    if (!audio_studio_vad_flush(handle, final_regions, 128, &written)) {
        std::cerr << "Recorded-speech flush failed: " << audio_studio_vad_last_error() << "\n";
        audio_studio_vad_destroy(handle);
        return 26;
    }
    regions.insert(regions.end(), final_regions, final_regions + written);
    audio_studio_vad_destroy(handle);

    const uint64_t duration_ms = static_cast<uint64_t>(recorded.size()) * 1000 / 16000;
    if (regions.empty()) {
        std::cerr << "Silero returned no speech regions for the recorded fixture.\n";
        return 27;
    }
    uint64_t previous_end = 0;
    for (const auto& region : regions) {
        if (region.start_ms >= region.end_ms || region.end_ms > duration_ms ||
            region.start_ms < previous_end) {
            std::cerr << "Recorded-speech regions are unordered or outside fixture duration.\n";
            return 28;
        }
        previous_end = region.end_ms;
    }

    std::cout << "Silero ONNX adapter and recorded-speech streaming validation passed.\n";
    return 0;
}
