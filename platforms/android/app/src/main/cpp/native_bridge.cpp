#include <jni.h>

#include <oboe/Oboe.h>

#include "audio_studio/audio_types.hpp"
#include "audio_studio/wav_writer.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace {

class AndroidRecorder final : public oboe::AudioStreamDataCallback,\n                             public oboe::AudioStreamErrorCallback {
public:
    static constexpr std::uint32_t kRequestedSampleRate = 48000U;
    static constexpr std::size_t kRingCapacity = 1U << 18U;
    static constexpr std::size_t kRingMask = kRingCapacity - 1U;

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
        for (std::uint64_t i = 0; i < frame_count; ++i) {
            ring_[static_cast<std::size_t>((write + i) & kRingMask)] = input[i];
        }
        write_index_.store(write + frame_count, std::memory_order_release);
        worker_cv_.notify_one();
        return oboe::DataCallbackResult::Continue;
    }

    void onErrorBeforeClose(oboe::AudioStream*, oboe::Result error) override {
        set_status(std::string("Microphone stream error: ") + oboe::convertToText(error));
        stop_requested_.store(true, std::memory_order_release);
        worker_cv_.notify_one();
    }

    void onErrorAfterClose(oboe::AudioStream*, oboe::Result error) override {
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
        const bool finalized = writer.finalize();
        std::string result = "Saved recording: " + path + " (" +
                             std::to_string(frames) + " frames).";
        if (overflow_.load(std::memory_order_acquire)) {
            result = "Recording stopped because the audio queue filled; the saved file may be incomplete: " + path;
        } else if (write_failed_.load(std::memory_order_acquire) || !finalized) {
            result = "Recording could not be fully written or finalized: " + path;
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
