#include "audio_studio/wav_processing.hpp"
#include "audio_studio/wav_reader.hpp"
#include "audio_studio/wav_writer.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <string>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

std::vector<unsigned char> load_bytes(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

bool has_partial_file_for(const std::string& path) {
    const auto prefix = std::filesystem::path(path).filename().string() + ".partial-";
    for (const auto& entry : std::filesystem::directory_iterator(".")) {
        if (entry.path().filename().string().rfind(prefix, 0U) == 0U) return true;
    }
    return false;
}

void test_processes_to_separate_file_and_preserves_source() {
    const std::string input = "audio_studio_processing_source.wav";
    const std::string output = "audio_studio_processing_result.wav";
    std::array<float, 4800> samples{};
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = i < 2400U ? 0.003F : 0.25F;
    }
    {
        audio_studio::WavWriter writer;
        require(writer.open(input, {48000U, 1U}), "create source WAV");
        require(writer.write_frames(samples.data(), samples.size()), "write source WAV");
        require(writer.finalize(), "finalize source WAV");
    }
    const auto original_bytes = load_bytes(input);
    std::string error;
    std::uint64_t last_progress = 0U;
    std::uint64_t reported_total = 0U;
    bool saw_initial_progress = false;
    const auto progress = [&](std::uint64_t processed, std::uint64_t total) {
        if (!saw_initial_progress) saw_initial_progress = processed == 0U && total == samples.size();
        require(processed >= last_progress, "progress must be monotonic");
        last_progress = processed;
        reported_total = total;
    };
    require(audio_studio::process_wav_with_noise_gate(input, output, {}, &error, nullptr, progress),
            "processing should create a separate WAV");
    require(error.empty(), "successful processing clears error");
    require(saw_initial_progress && last_progress == samples.size() && reported_total == samples.size(),
            "processing reports initial and final frame progress");
    require(load_bytes(input) == original_bytes, "source bytes must remain unchanged");

    audio_studio::AudioBuffer original;
    audio_studio::AudioBuffer processed;
    require(audio_studio::WavReader::read(input, original), "read original");
    require(audio_studio::WavReader::read(output, processed), "read processed output");
    require(processed.format.sample_rate_hz == original.format.sample_rate_hz &&
            processed.format.channel_count == original.format.channel_count,
            "processing preserves format");
    require(processed.samples.size() == original.samples.size(),
            "processing preserves duration and sample count");
    audio_studio::AudioBuffer expected = original;
    audio_studio::NoiseGate reference_gate;
    require(reference_gate.process(expected), "reference gate processes the whole buffer");
    for (std::size_t i = 0; i < processed.samples.size(); ++i) {
        require(std::abs(processed.samples[i] - expected.samples[i]) <= (1.5F / 32767.0F),
                "streamed output must match continuous whole-buffer gate processing");
    }
    double original_quiet = 0.0;
    double processed_quiet = 0.0;
    for (std::size_t i = 0; i < 2400U; ++i) {
        original_quiet += std::abs(original.samples[i]);
        processed_quiet += std::abs(processed.samples[i]);
    }
    require(processed_quiet < original_quiet, "gate should attenuate the quiet region");
    std::remove(input.c_str());
    std::remove(output.c_str());
}



void test_rnnoise_engine_processes_wav_and_preserves_source() {
    const std::string input = "audio_studio_rnnoise_source.wav";
    const std::string output = "audio_studio_rnnoise_result.wav";
    std::array<float, 9600U> samples{};
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const double t = static_cast<double>(i) / 48000.0;
        samples[i] = static_cast<float>(0.18 * std::sin(2.0 * 3.141592653589793 * 220.0 * t) +
                                        0.015 * std::sin(2.0 * 3.141592653589793 * 1837.0 * t));
    }
    {
        audio_studio::WavWriter writer;
        require(writer.open(input, {48000U, 1U}), "create RNNoise source WAV");
        require(writer.write_frames(samples.data(), samples.size()), "write RNNoise source WAV");
        require(writer.finalize(), "finalize RNNoise source WAV");
    }
    const auto original_bytes = load_bytes(input);
    std::string error;
    require(audio_studio::process_wav_with_noise_gate(
                input, output, {}, &error, nullptr, {},
                audio_studio::DspEffectsSettings{0.0F, 0.0F, true},
                audio_studio::ProcessingEngine::Rnnoise, 0.85F),
            "RNNoise should process a valid WAV");
    require(error.empty(), "RNNoise success clears processing error");
    require(load_bytes(input) == original_bytes, "RNNoise must preserve the source bytes");
    audio_studio::AudioBuffer processed;
    require(audio_studio::WavReader::read(output, processed), "RNNoise output should be a readable WAV");
    require(processed.format.sample_rate_hz == 48000U && processed.format.channel_count == 1U,
            "RNNoise output preserves the source audio format");
    require(!processed.samples.empty(), "RNNoise output must contain audio");
    for (const float sample : processed.samples) {
        require(std::isfinite(sample), "RNNoise output samples must remain finite");
    }
    std::remove(input.c_str());
    std::remove(output.c_str());
}


void test_truncated_source_does_not_publish_or_leave_partial_output() {
    const std::string input = "audio_studio_processing_truncated_source.wav";
    const std::string output = "audio_studio_processing_truncated_result.wav";
    const std::array<float, 64U> samples{};
    {
        audio_studio::WavWriter writer;
        require(writer.open(input, {48000U, 1U}), "create source for truncation test");
        require(writer.write_frames(samples.data(), samples.size()), "write truncation-test source");
        require(writer.finalize(), "finalize truncation-test source");
    }

    auto malformed = load_bytes(input);
    require(malformed.size() > 44U, "truncation fixture should contain audio payload");
    malformed.pop_back();
    {
        std::ofstream file(input, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(malformed.data()),
                   static_cast<std::streamsize>(malformed.size()));
        require(static_cast<bool>(file), "write truncated source fixture");
    }
    const auto source_before = load_bytes(input);

    std::string error;
    require(!audio_studio::process_wav_with_noise_gate(input, output, {}, &error),
            "processing must reject a source truncated inside its declared RIFF data");
    require(!error.empty(), "truncated input rejection should provide an explanation");
    require(load_bytes(input) == source_before,
            "processing failure must not modify the malformed source");
    require(!std::filesystem::exists(output),
            "truncated input must not publish a final processed WAV");
    require(!has_partial_file_for(output),
            "truncated input must not leave an orphaned output partial");

    std::remove(input.c_str());
    std::remove(output.c_str());
}

void test_cancellation_does_not_remove_concurrent_destination() {
    const std::string input = "audio_studio_processing_concurrent_cancel_source.wav";
    const std::string output = "audio_studio_processing_concurrent_cancel_result.wav";
    const std::array<float, 128U> samples{};
    {
        audio_studio::WavWriter writer;
        require(writer.open(input, {48000U, 1U}), "create concurrent-destination cancellation source");
        require(writer.write_frames(samples.data(), samples.size()),
                "write concurrent-destination cancellation source");
        require(writer.finalize(), "finalize concurrent-destination cancellation source");
    }
    const auto source_before = load_bytes(input);
    const std::vector<unsigned char> sentinel{'u','n','r','e','l','a','t','e','d'};
    std::atomic<bool> cancel{false};
    std::string error;
    const auto progress = [&](std::uint64_t processed, std::uint64_t /* total */) {
        if (processed == 0U) {
            std::ofstream file(output, std::ios::binary | std::ios::trunc);
            file.write(reinterpret_cast<const char*>(sentinel.data()),
                       static_cast<std::streamsize>(sentinel.size()));
            require(static_cast<bool>(file), "create concurrent destination sentinel");
            cancel.store(true, std::memory_order_release);
        }
    };
    require(!audio_studio::process_wav_with_noise_gate(
                input, output, {}, &error, &cancel, progress),
            "cancellation after another actor creates the destination must stop processing");
    require(error.find("cancelled") != std::string::npos,
            "concurrent-destination cancellation should be described");
    require(load_bytes(input) == source_before,
            "concurrent-destination cancellation must preserve source bytes");
    require(load_bytes(output) == sentinel,
            "failure cleanup must not delete an unrelated file at the destination path");
    require(!has_partial_file_for(output),
            "failure cleanup must still remove the writer-owned temporary file");

    std::remove(input.c_str());
    std::remove(output.c_str());
}

void test_cancellation_at_initial_progress_cleans_partial_output() {
    const std::string input = "audio_studio_processing_initial_cancel_source.wav";
    const std::string output = "audio_studio_processing_initial_cancel_result.wav";
    const std::array<float, 128U> samples{};
    {
        audio_studio::WavWriter writer;
        require(writer.open(input, {48000U, 1U}), "create initial-progress cancellation source");
        require(writer.write_frames(samples.data(), samples.size()),
                "write initial-progress cancellation source");
        require(writer.finalize(), "finalize initial-progress cancellation source");
    }
    const auto source_before = load_bytes(input);
    std::atomic<bool> cancel{false};
    std::string error;
    const auto progress = [&](std::uint64_t processed, std::uint64_t total) {
        require(processed == 0U && total == samples.size(),
                "first progress callback should report zero processed frames and the total");
        cancel.store(true, std::memory_order_release);
    };
    require(!audio_studio::process_wav_with_noise_gate(
                input, output, {}, &error, &cancel, progress),
            "cancellation requested by the initial progress callback must stop processing");
    require(error.find("cancelled") != std::string::npos,
            "initial-progress cancellation should provide a descriptive error");
    require(load_bytes(input) == source_before,
            "initial-progress cancellation must preserve the source bytes");
    require(!std::filesystem::exists(output),
            "initial-progress cancellation must not publish a final WAV");
    require(!has_partial_file_for(output),
            "initial-progress cancellation must remove the temporary WAV");

    std::remove(input.c_str());
    std::remove(output.c_str());
}

void test_cancellation_cleans_partial_output() {
    const std::string input = "audio_studio_processing_cancel_source.wav";
    const std::string output = "audio_studio_processing_cancel_result.wav";
    const std::array<float, 32> samples{};
    {
        audio_studio::WavWriter writer;
        require(writer.open(input, {48000U, 1U}), "create cancellation source");
        require(writer.write_frames(samples.data(), samples.size()), "write cancellation source");
        require(writer.finalize(), "finalize cancellation source");
    }
    std::atomic<bool> cancel{true};
    std::string error;
    require(!audio_studio::process_wav_with_noise_gate(input, output, {}, &error, &cancel),
            "pre-requested cancellation must stop processing");
    require(error.find("cancelled") != std::string::npos, "cancellation error should be descriptive");
    require(!std::ifstream(output).good(), "cancelled output must be removed");
    require(std::ifstream(input).good(), "cancellation must preserve source");
    std::remove(input.c_str());
    std::remove(output.c_str());
}

void test_cancellation_during_progress_cleans_partial_output() {
    const std::string input = "audio_studio_processing_mid_cancel_source.wav";
    const std::string output = "audio_studio_processing_mid_cancel_result.wav";
    std::array<float, 5000> samples{};
    samples.fill(0.2F);
    {
        audio_studio::WavWriter writer;
        require(writer.open(input, {48000U, 1U}), "create mid-cancellation source");
        require(writer.write_frames(samples.data(), samples.size()), "write mid-cancellation source");
        require(writer.finalize(), "finalize mid-cancellation source");
    }

    std::atomic<bool> cancel{false};
    std::string error;
    const auto progress = [&](std::uint64_t processed, std::uint64_t /* total */) {
        if (processed > 0U) cancel.store(true, std::memory_order_release);
    };
    require(!audio_studio::process_wav_with_noise_gate(
                input, output, {}, &error, &cancel, progress),
            "cancellation requested during processing must stop the operation");
    require(error.find("cancelled") != std::string::npos,
            "mid-processing cancellation error should be descriptive");
    require(!std::ifstream(output).good(),
            "mid-processing cancellation must remove partial output");
    require(std::ifstream(input).good(),
            "mid-processing cancellation must preserve source");
    std::remove(input.c_str());
    std::remove(output.c_str());
}


void test_progress_callback_exceptions_clean_partial_output() {
    const std::string input = "audio_studio_processing_callback_exception_source.wav";
    const std::string output = "audio_studio_processing_callback_exception_result.wav";
    std::array<float, 5000U> samples{};
    samples.fill(0.2F);
    {
        audio_studio::WavWriter writer;
        require(writer.open(input, {48000U, 1U}), "create callback-exception source");
        require(writer.write_frames(samples.data(), samples.size()), "write callback-exception source");
        require(writer.finalize(), "finalize callback-exception source");
    }
    const auto source_before = load_bytes(input);

    for (const bool throw_initial : {true, false}) {
        std::string error;
        bool threw_after_initial = false;
        const auto progress = [&](std::uint64_t processed, std::uint64_t) {
            if (throw_initial && processed == 0U) {
                throw std::runtime_error("initial callback test exception");
            }
            if (!throw_initial && processed > 0U && !threw_after_initial) {
                threw_after_initial = true;
                throw std::runtime_error("mid-processing callback test exception");
            }
        };
        require(!audio_studio::process_wav_with_noise_gate(
                    input, output, {}, &error, nullptr, progress),
                "a throwing progress callback must fail processing cleanly");
        require(error.find("Progress callback failed") != std::string::npos,
                "callback exceptions should become descriptive API errors");
        require(load_bytes(input) == source_before,
                "callback exceptions must not modify the source WAV");
        require(!std::filesystem::exists(output),
                "callback exceptions must not publish a final output");
        require(!has_partial_file_for(output),
                "callback exceptions must clean the writer-owned temporary file");
    }

    std::remove(input.c_str());
    std::remove(output.c_str());
}

void test_refuses_to_overwrite_input() {
    const std::string input = "audio_studio_processing_no_overwrite.wav";
    const std::array<float, 8> samples{0.1F, 0.2F, 0.3F, 0.4F, 0.5F, 0.6F, 0.7F, 0.8F};
    {
        audio_studio::WavWriter writer;
        require(writer.open(input, {48000U, 1U}), "create no-overwrite source");
        require(writer.write_frames(samples.data(), samples.size()), "write no-overwrite source");
        require(writer.finalize(), "finalize no-overwrite source");
    }
    const auto before = load_bytes(input);
    std::string error;
    require(!audio_studio::process_wav_with_noise_gate(input, input, {}, &error),
            "must reject same input/output path");
    require(!error.empty(), "no-overwrite error should be described");
    require(load_bytes(input) == before, "rejected operation must preserve source");
    std::remove(input.c_str());
}

void test_refuses_to_overwrite_existing_destination() {
    const std::string input = "audio_studio_processing_collision_source.wav";
    const std::string output = "audio_studio_processing_collision_output.wav";
    const std::array<float, 8> samples{0.1F, 0.2F, 0.3F, 0.4F, 0.5F, 0.6F, 0.7F, 0.8F};
    {
        audio_studio::WavWriter writer;
        require(writer.open(input, {48000U, 1U}), "create collision source");
        require(writer.write_frames(samples.data(), samples.size()), "write collision source");
        require(writer.finalize(), "finalize collision source");
    }
    const auto source_before = load_bytes(input);
    const std::vector<unsigned char> sentinel{'k', 'e', 'e', 'p', '-', 'm', 'e'};
    {
        std::ofstream file(output, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(sentinel.data()),
                   static_cast<std::streamsize>(sentinel.size()));
        require(static_cast<bool>(file), "create existing destination sentinel");
    }

    std::string error;
    require(!audio_studio::process_wav_with_noise_gate(input, output, {}, &error),
            "processing must refuse an existing destination");
    require(!error.empty(), "existing-destination refusal should explain the error");
    require(load_bytes(input) == source_before, "destination collision must preserve source bytes");
    require(load_bytes(output) == sentinel, "destination collision must preserve existing output bytes");

    std::remove(input.c_str());
    std::remove(output.c_str());
}

void test_refuses_relative_absolute_alias_of_input() {
    const std::string input = "audio_studio_processing_path_alias.wav";
    const std::array<float, 8> samples{0.1F, 0.2F, 0.3F, 0.4F, 0.5F, 0.6F, 0.7F, 0.8F};
    {
        audio_studio::WavWriter writer;
        require(writer.open(input, {48000U, 1U}), "create path-alias source");
        require(writer.write_frames(samples.data(), samples.size()), "write path-alias source");
        require(writer.finalize(), "finalize path-alias source");
    }
    const auto before = load_bytes(input);
    const auto absolute = std::filesystem::absolute(std::filesystem::path(input)).lexically_normal();
    std::string error;
    require(!audio_studio::process_wav_with_noise_gate(input, absolute.u8string(), {}, &error),
            "processing must reject a relative input and absolute output alias");
    require(!error.empty(), "path-alias refusal should explain the error");
    require(load_bytes(input) == before, "path-alias refusal must preserve source bytes");
    std::remove(input.c_str());
}



void test_trim_wav_writes_selected_frame_range_and_preserves_source() {
    const std::string input = "audio_studio_trim_source.wav";
    const std::string output = "audio_studio_trim_result.wav";
    std::array<float, 10U> samples{
        -0.9F, -0.7F, -0.5F, -0.3F, -0.1F,
         0.1F,  0.3F,  0.5F,  0.7F,  0.9F};
    {
        audio_studio::WavWriter writer;
        require(writer.open(input, {48000U, 1U}), "create trim source");
        require(writer.write_frames(samples.data(), samples.size()), "write trim source");
        require(writer.finalize(), "finalize trim source");
    }
    const auto source_before = load_bytes(input);
    std::string error;
    require(audio_studio::trim_wav(input, output, 3U, 7U, &error),
            "trim should write the selected half-open frame range");
    require(error.empty(), "successful trim clears error");
    require(load_bytes(input) == source_before, "trim must preserve source bytes");

    audio_studio::AudioBuffer trimmed;
    require(audio_studio::WavReader::read(output, trimmed, &error),
            "trimmed output should be a valid WAV");
    require(trimmed.format.sample_rate_hz == 48000U &&
            trimmed.format.channel_count == 1U,
            "trimmed output preserves source format");
    require(trimmed.samples.size() == 4U, "trim output contains end-start frames");
    for (std::size_t i = 0U; i < trimmed.samples.size(); ++i) {
        require(std::abs(trimmed.samples[i] - samples[i + 3U]) < 0.0001F,
                "trim output samples match selected source range");
    }

    require(!audio_studio::trim_wav(input, input, 1U, 2U, &error),
            "trim must refuse to overwrite source");
    require(load_bytes(input) == source_before, "overwrite refusal preserves source");
    require(!audio_studio::trim_wav(input, output, 0U, 1U, &error),
            "trim must refuse to overwrite an existing destination");
    require(!audio_studio::trim_wav(input, "audio_studio_trim_invalid.wav", 8U, 11U, &error),
            "trim must reject a range beyond the source length");
    require(!std::filesystem::exists("audio_studio_trim_invalid.wav"),
            "invalid range must not publish an output");
    require(!audio_studio::trim_wav(input, "audio_studio_trim_empty.wav", 4U, 4U, &error),
            "trim must reject an empty range");

    std::remove(input.c_str());
    std::remove(output.c_str());
    std::remove("audio_studio_trim_invalid.wav");
    std::remove("audio_studio_trim_empty.wav");
}

void test_waveform_analysis_returns_bounded_min_max_envelope() {
    const std::string input = "audio_studio_waveform_envelope.wav";
    std::array<float, 400U> samples{};
    for (std::size_t i = 0U; i < samples.size(); ++i) {
        const std::size_t region = i / 100U;
        samples[i] = region == 0U ? 0.1F : region == 1U ? -0.35F :
                     region == 2U ? 0.6F : (i % 2U == 0U ? -0.8F : 0.8F);
    }
    {
        audio_studio::WavWriter writer;
        require(writer.open(input, {48000U, 1U}), "create waveform analysis source");
        require(writer.write_frames(samples.data(), samples.size()), "write waveform analysis source");
        require(writer.finalize(), "finalize waveform analysis source");
    }

    std::vector<audio_studio::WaveformPeak> peaks;
    audio_studio::AudioFormat format{};
    std::uint64_t frames = 0U;
    std::string error;
    require(audio_studio::analyze_wav_waveform(input, 4U, peaks, &format, &frames, &error),
            "waveform analysis should succeed");
    require(error.empty(), "successful waveform analysis clears error");
    require(peaks.size() == 4U, "waveform analysis returns requested bucket count");
    require(frames == samples.size(), "waveform analysis reports total frame count");
    require(format.sample_rate_hz == 48000U && format.channel_count == 1U,
            "waveform analysis reports source format");
    require(peaks[0].min_left > 0.09F && peaks[0].max_left > 0.09F,
            "first waveform bucket represents the quiet positive signal");
    require(peaks[1].min_left < -0.34F && peaks[1].max_left < -0.34F,
            "second waveform bucket preserves negative peak polarity");
    require(peaks[2].max_left > 0.59F, "third waveform bucket preserves positive peaks");
    require(peaks[3].min_left < -0.79F && peaks[3].max_left > 0.79F,
            "fourth waveform bucket captures both extrema");
    require(peaks[3].min_right == peaks[3].min_left &&
            peaks[3].max_right == peaks[3].max_left,
            "mono waveform mirrors its envelope into the right channel");
    require(!audio_studio::analyze_wav_waveform(input, 0U, peaks, nullptr, nullptr, &error),
            "zero waveform buckets must be rejected");
    require(peaks.empty() && !error.empty(), "invalid waveform request clears output and explains failure");
    std::remove(input.c_str());
}


void test_pcm24_wav_workflows() {
    const std::string input = "audio_studio_pcm24.wav";
    const std::string trimmed = "audio_studio_pcm24_trim.wav";
    const std::string processed = "audio_studio_pcm24_processed.wav";
    std::vector<unsigned char> bytes{'R','I','F','F'};
    auto put16 = [&](std::uint16_t v) { bytes.push_back(v & 255U); bytes.push_back((v >> 8U) & 255U); };
    auto put32 = [&](std::uint32_t v) { for (unsigned int i=0;i<4U;++i) bytes.push_back((v >> (8U*i)) & 255U); };
    put32(48U);
    bytes.insert(bytes.end(), {'W','A','V','E','f','m','t',' '});
    put32(16U); put16(1U); put16(1U); put32(48000U); put32(144000U); put16(3U); put16(24U);
    bytes.insert(bytes.end(), {'d','a','t','a'}); put32(12U);
    for (int i=0;i<4;++i) { bytes.push_back(0U); bytes.push_back(0U); bytes.push_back(i%2==0 ? 0x40U : 0xc0U); }
    {
        std::ofstream file(input, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        require(static_cast<bool>(file), "write PCM24 fixture");
    }
    std::string error;
    std::vector<audio_studio::WaveformPeak> peaks;
    std::uint64_t frames=0U;
    require(audio_studio::analyze_wav_waveform(input, 4U, peaks, nullptr, &frames, &error),
            "waveform analysis accepts PCM24");
    require(frames==4U, "PCM24 frame count is correct");
    require(audio_studio::trim_wav(input, trimmed, 1U, 3U, &error), "trim accepts PCM24");
    audio_studio::AudioBuffer result;
    require(audio_studio::WavReader::read(trimmed, result), "PCM24 trim exports readable PCM16");
    require(result.frame_count()==2U, "PCM24 trim preserves selected frames");
    require(audio_studio::process_wav_with_noise_gate(input, processed, {}, &error),
            "processing accepts PCM24");
    require(audio_studio::WavReader::read(processed, result), "PCM24 processing exports readable PCM16");
    std::remove(input.c_str()); std::remove(trimmed.c_str()); std::remove(processed.c_str());
}


void test_extensible_pcm24_wav_workflows() {
    const std::string input = "audio_studio_extensible_pcm24.wav";
    const std::string trimmed = "audio_studio_extensible_pcm24_trim.wav";
    const std::string processed = "audio_studio_extensible_pcm24_processed.wav";
    std::vector<unsigned char> bytes{'R','I','F','F'};
    auto put16 = [&](std::uint16_t v) { bytes.push_back(static_cast<unsigned char>(v & 255U)); bytes.push_back(static_cast<unsigned char>((v >> 8U) & 255U)); };
    auto put32 = [&](std::uint32_t v) { for (unsigned int i=0;i<4U;++i) bytes.push_back(static_cast<unsigned char>((v >> (8U*i)) & 255U)); };
    put32(72U);
    bytes.insert(bytes.end(), {'W','A','V','E','f','m','t',' '});
    put32(40U); put16(0xfffeU); put16(1U); put32(48000U); put32(144000U); put16(3U); put16(24U);
    put16(22U); put16(24U); put32(0U);
    bytes.insert(bytes.end(), {0x01U,0x00U,0x00U,0x00U,0x00U,0x00U,0x10U,0x00U,
                               0x80U,0x00U,0x00U,0xaaU,0x00U,0x38U,0x9bU,0x71U});
    bytes.insert(bytes.end(), {'d','a','t','a'}); put32(12U);
    for (int i=0;i<4;++i) { bytes.push_back(0U); bytes.push_back(0U); bytes.push_back(i%2==0 ? 0x40U : 0xc0U); }
    {
        std::ofstream file(input, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        require(static_cast<bool>(file), "write extensible PCM24 fixture");
    }
    std::string error;
    std::vector<audio_studio::WaveformPeak> peaks;
    std::uint64_t frames=0U;
    require(audio_studio::analyze_wav_waveform(input, 4U, peaks, nullptr, &frames, &error),
            "waveform analysis accepts WAVE_FORMAT_EXTENSIBLE PCM24");
    require(frames==4U, "extensible PCM24 frame count is correct");
    require(audio_studio::trim_wav(input, trimmed, 1U, 3U, &error),
            "trim accepts WAVE_FORMAT_EXTENSIBLE PCM24");
    audio_studio::AudioBuffer result;
    require(audio_studio::WavReader::read(trimmed, result), "extensible PCM24 trim exports readable PCM16");
    require(result.frame_count()==2U, "extensible PCM24 trim preserves selected frames");
    require(audio_studio::process_wav_with_noise_gate(input, processed, {}, &error),
            "processing accepts WAVE_FORMAT_EXTENSIBLE PCM24");
    require(audio_studio::WavReader::read(processed, result),
            "extensible PCM24 processing exports readable PCM16");
    std::remove(input.c_str()); std::remove(trimmed.c_str()); std::remove(processed.c_str());
}


void test_extensible_wav_rejects_oversized_extension_declaration() {
    const std::string input = "audio_studio_extensible_bad_extension.wav";
    std::vector<unsigned char> bytes{'R','I','F','F'};
    auto put16 = [&](std::uint16_t v) { bytes.push_back(static_cast<unsigned char>(v & 255U)); bytes.push_back(static_cast<unsigned char>((v >> 8U) & 255U)); };
    auto put32 = [&](std::uint32_t v) { for (unsigned int i=0;i<4U;++i) bytes.push_back(static_cast<unsigned char>((v >> (8U*i)) & 255U)); };
    put32(72U);
    bytes.insert(bytes.end(), {'W','A','V','E','f','m','t',' '});
    put32(40U); put16(0xfffeU); put16(1U); put32(48000U); put32(144000U); put16(3U); put16(24U);
    put16(23U); put16(24U); put32(0U);
    bytes.insert(bytes.end(), {0x01U,0x00U,0x00U,0x00U,0x00U,0x00U,0x10U,0x00U,
                               0x80U,0x00U,0x00U,0xaaU,0x00U,0x38U,0x9bU,0x71U});
    bytes.insert(bytes.end(), {'d','a','t','a'}); put32(12U);
    for (int i=0;i<4;++i) { bytes.push_back(0U); bytes.push_back(0U); bytes.push_back(i%2==0 ? 0x40U : 0xc0U); }
    {
        std::ofstream file(input, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        require(static_cast<bool>(file), "write malformed extensible WAV fixture");
    }
    std::vector<audio_studio::WaveformPeak> peaks;
    std::string error;
    require(!audio_studio::analyze_wav_waveform(input, 4U, peaks, nullptr, nullptr, &error),
            "WAVE_FORMAT_EXTENSIBLE must reject cbSize extending beyond the fmt chunk");
    require(!error.empty(), "malformed extensible header reports an explanatory error");
    std::remove(input.c_str());
}

}  // namespace

int main() {
    test_extensible_pcm24_wav_workflows();
    test_extensible_wav_rejects_oversized_extension_declaration();
    test_pcm24_wav_workflows();
    test_processes_to_separate_file_and_preserves_source();
    test_rnnoise_engine_processes_wav_and_preserves_source();
    test_waveform_analysis_returns_bounded_min_max_envelope();
    test_trim_wav_writes_selected_frame_range_and_preserves_source();
    test_truncated_source_does_not_publish_or_leave_partial_output();
    test_cancellation_does_not_remove_concurrent_destination();
    test_cancellation_at_initial_progress_cleans_partial_output();
    test_cancellation_cleans_partial_output();
    test_cancellation_during_progress_cleans_partial_output();
    test_progress_callback_exceptions_clean_partial_output();
    test_refuses_to_overwrite_input();
    test_refuses_to_overwrite_existing_destination();
    test_refuses_relative_absolute_alias_of_input();
    std::cout << "All WAV processing tests passed.\n";
    return EXIT_SUCCESS;
}
