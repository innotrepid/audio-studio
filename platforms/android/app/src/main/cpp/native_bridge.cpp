#include <jni.h>

#include <oboe/Oboe.h>

#include "audio_studio/audio_types.hpp"
#include "audio_studio/wav_writer.hpp"
#include "audio_studio/wav_processing.hpp"
#include "audio_studio/wav_recovery.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

class AndroidRecorder final : public oboe::AudioStreamDataCallback,
                             public oboe::AudioStreamErrorCallback {
public:
    static constexpr std::uint32_t kRequestedSampleRate = 48000U;
    static constexpr std::size_t kRingCapacity = 1U << 18U;
    static constexpr std::size_t kRingMask = kRingCapacity - 1U;

    std::array<float, 3U> meter() const noexcept {
        return {meter_rms_.load(std::memory_order_relaxed),
                meter_peak_.load(std::memory_order_relaxed),
                clipping_detected_.load(std::memory_order_relaxed) ? 1.0F : 0.0F};
    }

    bool start(const std::string& path) {
        if (running_.load(std::memory_order_acquire)) {
            set_status("A recording is already active.");
            return false;
        }

        oboe::AudioStreamBuilder builder;
        builder.setDirection(oboe::Direction::Input);
        builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
        builder.setSharingMode(oboe::SharingMode::Shared);
        builder.setFormat(oboe::AudioFormat::Float);
        builder.setChannelCount(1);
        builder.setSampleRate(static_cast<int32_t>(kRequestedSampleRate));
        builder.setDataCallback(this);
        builder.setErrorCallback(this);

        const auto open_result = builder.openStream(stream_);
        if (open_result != oboe::Result::OK || !stream_) {
            set_status(std::string("Could not open microphone stream: ") +
                       oboe::convertToText(open_result));
            stream_.reset();
            return false;
        }

        const auto actual_sample_rate =
            static_cast<std::uint32_t>(stream_->getSampleRate());
        if (actual_sample_rate == 0U) {
            stream_->close();
            stream_.reset();
            set_status("Microphone returned an invalid sample rate.");
            return false;
        }

        stop_requested_.store(false, std::memory_order_release);
        overflow_.store(false, std::memory_order_release);
        write_failed_.store(false, std::memory_order_release);
        stream_error_.store(false, std::memory_order_release);
        meter_rms_.store(0.0F, std::memory_order_release);
        meter_peak_.store(0.0F, std::memory_order_release);
        clipping_detected_.store(false, std::memory_order_release);
        write_index_.store(0U, std::memory_order_release);
        read_index_.store(0U, std::memory_order_release);
        worker_ready_ = false;
        worker_opened_ = false;

        worker_ = std::thread(&AndroidRecorder::writer_loop, this, path,
                              audio_studio::AudioFormat{actual_sample_rate, 1U});
        {
            std::unique_lock<std::mutex> lock(worker_mutex_);
            worker_cv_.wait(lock, [this] { return worker_ready_; });
        }
        if (!worker_opened_) {
            if (worker_.joinable()) worker_.join();
            stream_->close();
            stream_.reset();
            return false;
        }

        const auto start_result = stream_->requestStart();
        if (start_result != oboe::Result::OK) {
            stream_->close();
            stream_.reset();
            stop_requested_.store(true, std::memory_order_release);
            worker_cv_.notify_one();
            if (worker_.joinable()) worker_.join();
            set_status(std::string("Could not start microphone stream: ") +
                       oboe::convertToText(start_result));
            return false;
        }

        running_.store(true, std::memory_order_release);
        set_status("Recording");
        return true;
    }

    std::string stop() {
        if (stream_) {
            (void)stream_->requestStop();
            (void)stream_->close();
            stream_.reset();
        }

        stop_requested_.store(true, std::memory_order_release);
        worker_cv_.notify_one();
        if (worker_.joinable()) worker_.join();
        running_.store(false, std::memory_order_release);

        std::lock_guard<std::mutex> lock(status_mutex_);
        return final_status_;
    }

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream*,
                                           void* audio_data,
                                           int32_t num_frames) override {
        if (num_frames <= 0 || audio_data == nullptr) {
            return oboe::DataCallbackResult::Continue;
        }

        const auto frame_count = static_cast<std::uint64_t>(num_frames);
        const auto write = write_index_.load(std::memory_order_relaxed);
        const auto read = read_index_.load(std::memory_order_acquire);
        const auto queued = write - read;
        if (frame_count > kRingCapacity || queued > kRingCapacity - frame_count) {
            overflow_.store(true, std::memory_order_release);
            worker_cv_.notify_one();
            return oboe::DataCallbackResult::Stop;
        }

        const auto* input = static_cast<const float*>(audio_data);
        float peak = 0.0F;
        double sum_squares = 0.0;
        for (std::uint64_t i = 0; i < frame_count; ++i) {
            const float sample = input[i];
            ring_[static_cast<std::size_t>((write + i) & kRingMask)] = sample;
            if (std::isfinite(sample)) {
                const float magnitude = std::abs(sample);
                peak = std::max(peak, magnitude);
                sum_squares += static_cast<double>(sample) * static_cast<double>(sample);
                if (magnitude >= 0.999F) {
                    clipping_detected_.store(true, std::memory_order_release);
                }
            }
        }
        meter_peak_.store(peak, std::memory_order_release);
        meter_rms_.store(static_cast<float>(std::sqrt(sum_squares / static_cast<double>(frame_count))),
                         std::memory_order_release);
        write_index_.store(write + frame_count, std::memory_order_release);
        worker_cv_.notify_one();
        return oboe::DataCallbackResult::Continue;
    }

    void onErrorBeforeClose(oboe::AudioStream*, oboe::Result error) override {
        stream_error_.store(true, std::memory_order_release);
        set_status(std::string("Microphone stream error: ") + oboe::convertToText(error));
        stop_requested_.store(true, std::memory_order_release);
        worker_cv_.notify_one();
    }

    void onErrorAfterClose(oboe::AudioStream*, oboe::Result error) override {
        stream_error_.store(true, std::memory_order_release);
        set_status(std::string("Microphone stream closed with error: ") +
                   oboe::convertToText(error));
        stop_requested_.store(true, std::memory_order_release);
        worker_cv_.notify_one();
    }

private:
    void writer_loop(const std::string& path, audio_studio::AudioFormat format) {
        audio_studio::WavWriter writer;
        const bool opened = writer.open(path, format);
        {
            std::lock_guard<std::mutex> lock(worker_mutex_);
            worker_opened_ = opened;
            worker_ready_ = true;
        }
        worker_cv_.notify_all();
        if (!opened) {
            set_status(std::string("Could not create WAV recording: ") + writer.last_error());
            return;
        }

        for (;;) {
            {
                std::unique_lock<std::mutex> lock(worker_mutex_);
                worker_cv_.wait_for(lock, std::chrono::milliseconds(50), [this] {
                    return stop_requested_.load(std::memory_order_acquire) ||
                           overflow_.load(std::memory_order_acquire) ||
                           write_index_.load(std::memory_order_acquire) !=
                               read_index_.load(std::memory_order_relaxed);
                });
            }

            auto read = read_index_.load(std::memory_order_relaxed);
            const auto write = write_index_.load(std::memory_order_acquire);
            while (read < write) {
                const auto offset = static_cast<std::size_t>(read & kRingMask);
                const auto available = static_cast<std::size_t>(write - read);
                const auto contiguous = std::min(available, kRingCapacity - offset);
                if (!writer.write_frames(ring_.data() + offset, contiguous)) {
                    write_failed_.store(true, std::memory_order_release);
                    stop_requested_.store(true, std::memory_order_release);
                    break;
                }
                read += contiguous;
                read_index_.store(read, std::memory_order_release);
            }

            const bool stopping =
                stop_requested_.load(std::memory_order_acquire) ||
                overflow_.load(std::memory_order_acquire) ||
                write_failed_.load(std::memory_order_acquire);
            if (stopping &&
                read_index_.load(std::memory_order_acquire) ==
                    write_index_.load(std::memory_order_acquire)) {
                break;
            }
        }

        const auto frames = writer.frames_written();
        const bool overflowed = overflow_.load(std::memory_order_acquire);
        const bool stream_failed = stream_error_.load(std::memory_order_acquire);
        const bool write_failed = write_failed_.load(std::memory_order_acquire);
        bool finalized = false;
        if (overflowed || stream_failed || write_failed) {
            // Do not publish a plausible-looking WAV after a capture or disk-write failure.
            writer.abort();
        } else {
            finalized = writer.finalize();
            if (!finalized) writer.abort();
        }

        std::string result = "Saved recording: " + path + " (" +
                             std::to_string(frames) + " frames).";
        if (overflowed) {
            result = "ERROR: Recording stopped because the audio queue filled; incomplete output was discarded.";
        } else if (stream_failed) {
            result = "ERROR: The microphone stream was interrupted; incomplete output was discarded.";
        } else if (write_failed || !finalized) {
            result = "ERROR: Recording could not be fully written or finalized; incomplete output was discarded.";
        }
        set_status(std::move(result));
    }

    void set_status(std::string status) {
        std::lock_guard<std::mutex> lock(status_mutex_);
        final_status_ = std::move(status);
    }

    std::array<float, kRingCapacity> ring_{};
    std::atomic<std::uint64_t> write_index_{0U};
    std::atomic<std::uint64_t> read_index_{0U};
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> overflow_{false};
    std::atomic<bool> write_failed_{false};
    std::atomic<bool> stream_error_{false};
    std::atomic<float> meter_rms_{0.0F};
    std::atomic<float> meter_peak_{0.0F};
    std::atomic<bool> clipping_detected_{false};
    std::atomic<bool> running_{false};
    std::shared_ptr<oboe::AudioStream> stream_{};
    std::thread worker_{};
    std::mutex worker_mutex_{};
    std::condition_variable worker_cv_{};
    bool worker_ready_{false};
    bool worker_opened_{false};
    std::mutex status_mutex_{};
    std::string final_status_{"Ready to record."};
};

std::mutex g_recorder_mutex;
std::unique_ptr<AndroidRecorder> g_recorder;
std::atomic<bool> g_cancel_noise_gate{false};

}  // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeEngineStatus(
    JNIEnv* env, jobject /* thiz */) {
    const audio_studio::AudioFormat baseline{48000U, 1U};
    const char* status = baseline.is_valid()
        ? "Native C++ core connected · Oboe microphone capture available"
        : "Native audio core failed its baseline validation";
    return env->NewStringUTF(status);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeStartRecording(
    JNIEnv* env, jobject /* thiz */, jstring output_path) {
    if (output_path == nullptr) return JNI_FALSE;
    const char* chars = env->GetStringUTFChars(output_path, nullptr);
    if (chars == nullptr) return JNI_FALSE;
    const std::string path(chars);
    env->ReleaseStringUTFChars(output_path, chars);

    std::lock_guard<std::mutex> lock(g_recorder_mutex);
    if (g_recorder) return JNI_FALSE;
    auto recorder = std::make_unique<AndroidRecorder>();
    if (!recorder->start(path)) {
        (void)recorder->stop();
        return JNI_FALSE;
    }
    g_recorder = std::move(recorder);
    return JNI_TRUE;
}

extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeRecordingMeter(
    JNIEnv* env, jobject /* thiz */) {
    std::array<float, 3U> values{};
    {
        std::lock_guard<std::mutex> lock(g_recorder_mutex);
        if (g_recorder) values = g_recorder->meter();
    }
    jfloatArray result = env->NewFloatArray(static_cast<jsize>(values.size()));
    if (result != nullptr) {
        env->SetFloatArrayRegion(result, 0, static_cast<jsize>(values.size()), values.data());
    }
    return result;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeStopRecording(
    JNIEnv* env, jobject /* thiz */) {
    std::unique_ptr<AndroidRecorder> recorder;
    {
        std::lock_guard<std::mutex> lock(g_recorder_mutex);
        recorder = std::move(g_recorder);
    }
    if (!recorder) return env->NewStringUTF("No recording is active.");
    const auto status = recorder->stop();
    return env->NewStringUTF(status.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativePrepareNoiseGateProcessing(
    JNIEnv* /* env */, jobject /* thiz */) {
    g_cancel_noise_gate.store(false, std::memory_order_release);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeProcessNoiseGate(
    JNIEnv* env, jobject /* thiz */, jstring input_path, jstring output_path,
    jfloat threshold_dbfs, jfloat attenuation_db, jfloat gain_db, jfloat high_pass_hz,
    jobject progress_callback) {
    if (input_path == nullptr || output_path == nullptr) {
        return env->NewStringUTF("ERROR: Input and output paths are required.");
    }
    const char* input_chars = env->GetStringUTFChars(input_path, nullptr);
    if (input_chars == nullptr) return env->NewStringUTF("ERROR: Could not read input path.");
    const std::string input(input_chars);
    env->ReleaseStringUTFChars(input_path, input_chars);

    const char* output_chars = env->GetStringUTFChars(output_path, nullptr);
    if (output_chars == nullptr) return env->NewStringUTF("ERROR: Could not read output path.");
    const std::string output(output_chars);
    env->ReleaseStringUTFChars(output_path, output_chars);

    jclass callback_class = progress_callback == nullptr ? nullptr : env->GetObjectClass(progress_callback);
    jmethodID progress_method = callback_class == nullptr ? nullptr :
        env->GetMethodID(callback_class, "onProgress", "(JJ)V");

    audio_studio::NoiseGateSettings settings{};
    settings.threshold_dbfs = threshold_dbfs;
    settings.attenuation_db = attenuation_db;

    std::string error;
    const auto progress = [env, progress_callback, progress_method](std::uint64_t processed, std::uint64_t total) {
        if (progress_callback != nullptr && progress_method != nullptr) {
            env->CallVoidMethod(progress_callback, progress_method,
                                static_cast<jlong>(processed), static_cast<jlong>(total));
        }
    };
    if (!audio_studio::process_wav_with_noise_gate(
            input, output, settings, &error, &g_cancel_noise_gate, progress,
            audio_studio::DspEffectsSettings{gain_db, high_pass_hz, true})) {
        const std::string message = "ERROR: " + error;
        return env->NewStringUTF(message.c_str());
    }
    const std::string message = "Processed WAV: " + output;
    return env->NewStringUTF(message.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeCancelNoiseGateProcessing(
    JNIEnv* /* env */, jobject /* thiz */) {
    g_cancel_noise_gate.store(true, std::memory_order_release);
}

extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeAnalyzeWavWaveform(
    JNIEnv* env, jobject /* thiz */, jstring input_path, jint bucket_count) {
    if (input_path == nullptr || bucket_count <= 0 || bucket_count > 1000000) return env->NewFloatArray(0);
    const char* chars = env->GetStringUTFChars(input_path, nullptr);
    if (chars == nullptr) return env->NewFloatArray(0);
    const std::string input(chars);
    env->ReleaseStringUTFChars(input_path, chars);
    std::vector<audio_studio::WaveformPeak> peaks;
    std::string error;
    if (!audio_studio::analyze_wav_waveform(input, static_cast<std::size_t>(bucket_count), peaks, nullptr, nullptr, &error))
        return env->NewFloatArray(0);
    std::vector<jfloat> values;
    values.reserve(peaks.size() * 4U);
    for (const auto& peak : peaks) {
        values.push_back(peak.min_left);
        values.push_back(peak.max_left);
        values.push_back(peak.min_right);
        values.push_back(peak.max_right);
    }
    jfloatArray result = env->NewFloatArray(static_cast<jsize>(values.size()));
    if (result != nullptr && !values.empty())
        env->SetFloatArrayRegion(result, 0, static_cast<jsize>(values.size()), values.data());
    return result;
}



extern "C" JNIEXPORT jstring JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeWavAnalysisError(
    JNIEnv* env, jobject /* thiz */, jstring input_path) {
    if (input_path == nullptr) return env->NewStringUTF("ERROR: No WAV file path was provided.");
    const char* chars = env->GetStringUTFChars(input_path, nullptr);
    if (chars == nullptr) return env->NewStringUTF("ERROR: Could not read the selected file path.");
    const std::string input(chars);
    env->ReleaseStringUTFChars(input_path, chars);
    std::vector<audio_studio::WaveformPeak> peaks;
    std::string error;
    if (audio_studio::analyze_wav_waveform(input, 1U, peaks, nullptr, nullptr, &error)) {
        return env->NewStringUTF("OK");
    }
    const std::string message = "ERROR: " + error;
    return env->NewStringUTF(message.c_str());
}

extern "C" JNIEXPORT jlongArray JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeGetWavMetadata(
    JNIEnv* env, jobject /* thiz */, jstring input_path) {
    if (input_path == nullptr) return env->NewLongArray(0);
    const char* chars = env->GetStringUTFChars(input_path, nullptr);
    if (chars == nullptr) return env->NewLongArray(0);
    const std::string input(chars);
    env->ReleaseStringUTFChars(input_path, chars);
    std::vector<audio_studio::WaveformPeak> peaks;
    audio_studio::AudioFormat format{};
    std::uint64_t total_frames = 0U;
    std::string error;
    if (!audio_studio::analyze_wav_waveform(input, 1U, peaks, &format, &total_frames, &error))
        return env->NewLongArray(0);
    const jlong values[3] = {
        static_cast<jlong>(total_frames),
        static_cast<jlong>(format.sample_rate_hz),
        static_cast<jlong>(format.channel_count)
    };
    jlongArray result = env->NewLongArray(3);
    if (result != nullptr) env->SetLongArrayRegion(result, 0, 3, values);
    return result;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeTrimWav(
    JNIEnv* env, jobject /* thiz */, jstring input_path, jstring output_path,
    jlong start_frame, jlong end_frame) {
    if (input_path == nullptr || output_path == nullptr || start_frame < 0 ||
        end_frame <= start_frame) {
        return env->NewStringUTF("ERROR: Choose a valid non-empty trim range.");
    }
    const char* input_chars = env->GetStringUTFChars(input_path, nullptr);
    if (input_chars == nullptr) return env->NewStringUTF("ERROR: Could not read source path.");
    const std::string input(input_chars);
    env->ReleaseStringUTFChars(input_path, input_chars);
    const char* output_chars = env->GetStringUTFChars(output_path, nullptr);
    if (output_chars == nullptr) return env->NewStringUTF("ERROR: Could not read destination path.");
    const std::string output(output_chars);
    env->ReleaseStringUTFChars(output_path, output_chars);
    std::vector<audio_studio::WaveformPeak> metadata_peaks;
    std::uint64_t total_frames = 0U;
    std::string error;
    if (!audio_studio::analyze_wav_waveform(input, 1U, metadata_peaks, nullptr,
                                            &total_frames, &error)) {
        const std::string message = "ERROR: " + error;
        return env->NewStringUTF(message.c_str());
    }
    const auto start_frame_value = static_cast<std::uint64_t>(start_frame);
    const auto end_frame_value = static_cast<std::uint64_t>(end_frame);
    if (end_frame_value > total_frames) {
        return env->NewStringUTF("ERROR: The selected frame range exceeds the source WAV.");
    }
    if (!audio_studio::trim_wav(input, output, start_frame_value, end_frame_value, &error)) {
        const std::string message = "ERROR: " + error;
        return env->NewStringUTF(message.c_str());
    }
    return env->NewStringUTF("TRIMMED");
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_innotrepid_audiostudio_MainActivity_nativeRecoverPartialWav(
    JNIEnv* env, jobject /* thiz */, jstring partial_path, jstring output_path) {
    if (partial_path == nullptr || output_path == nullptr) {
        return env->NewStringUTF("ERROR: Recovery paths are required.");
    }
    const char* partial_chars = env->GetStringUTFChars(partial_path, nullptr);
    if (partial_chars == nullptr) return env->NewStringUTF("ERROR: Could not read temporary path.");
    const std::string partial(partial_chars);
    env->ReleaseStringUTFChars(partial_path, partial_chars);

    const char* output_chars = env->GetStringUTFChars(output_path, nullptr);
    if (output_chars == nullptr) return env->NewStringUTF("ERROR: Could not read destination path.");
    const std::string output(output_chars);
    env->ReleaseStringUTFChars(output_path, output_chars);

    std::string error;
    if (!audio_studio::recover_partial_wav(partial, output, &error)) {
        const std::string message = "ERROR: " + error;
        return env->NewStringUTF(message.c_str());
    }
    return env->NewStringUTF("RECOVERED");
}

