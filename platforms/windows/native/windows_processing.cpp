#include "audio_studio/native_api.h"
#include "audio_studio/wav_processing.hpp"

#include <windows.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {
std::string utf8_path(const wchar_t* value) {
    if (value == nullptr || value[0] == L'\0') return {};
    const int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1,
                                              nullptr, 0, nullptr, nullptr);
    if (required <= 1) return {};
    std::string result(static_cast<std::size_t>(required), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1,
                            result.data(), required, nullptr, nullptr) == 0) return {};
    result.resize(static_cast<std::size_t>(required - 1));
    return result;
}

std::wstring widen_utf8(const std::string& value) {
    if (value.empty()) return {};
    const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                              value.c_str(), -1, nullptr, 0);
    if (required <= 1) return L"";
    std::wstring result(static_cast<std::size_t>(required), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), -1,
                            result.data(), required) == 0) return L"";
    result.resize(static_cast<std::size_t>(required - 1));
    return result;
}

class WindowsProcessor final {
public:
    ~WindowsProcessor() {
        cancel_.store(true);
        if (worker_.joinable()) worker_.join();
    }

    int start(const wchar_t* input, const wchar_t* output,
              float threshold_dbfs, float attenuation_db, float gain_db, float high_pass_hz) {
        const std::string input_utf8 = utf8_path(input);
        const std::string output_utf8 = utf8_path(output);
        if (input_utf8.empty() || output_utf8.empty()) {
            set_status(L"Choose valid input and output paths.");
            return 0;
        }

        std::unique_lock<std::mutex> lock(mutex_);
        if (worker_.joinable()) {
            if (active_.load()) {
                status_ = L"Processing is already active.";
                return 0;
            }
            lock.unlock();
            worker_.join();
            lock.lock();
        }
        cancel_.store(false);
        progress_.store(0.0F);
        active_.store(true);
        status_ = L"Preparing WAV processing…";
        worker_ = std::thread(&WindowsProcessor::run, this, input_utf8, output_utf8,
                              threshold_dbfs, attenuation_db, gain_db, high_pass_hz);
        return 1;
    }

    void cancel() {
        cancel_.store(true);
    }

    int active() const noexcept { return active_.load() ? 1 : 0; }
    float progress() const noexcept { return progress_.load(); }

    const wchar_t* status() {
        thread_local std::wstring snapshot;
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot = status_;
        return snapshot.c_str();
    }

private:
    void set_status(const std::wstring& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        status_ = value;
    }

    void run(const std::string& input, const std::string& output,
             float threshold_dbfs, float attenuation_db, float gain_db, float high_pass_hz) {
        audio_studio::NoiseGateSettings settings;
        settings.threshold_dbfs = threshold_dbfs;
        settings.attenuation_db = attenuation_db;
        std::string error;
        const bool succeeded = audio_studio::process_wav_with_noise_gate(
            input, output, settings, &error, &cancel_,
            [this](std::uint64_t done, std::uint64_t total) {
                progress_.store(total == 0U ? 0.0F :
                    static_cast<float>(static_cast<double>(done) / static_cast<double>(total)));
            }, audio_studio::DspEffectsSettings{gain_db, high_pass_hz, true});
        if (succeeded) {
            progress_.store(1.0F);
            set_status(L"Processed WAV saved. Original recording was preserved.");
        } else if (cancel_.load()) {
            set_status(L"Processing cancelled. Incomplete output was discarded.");
        } else {
            const std::wstring message = widen_utf8(error);
            set_status(message.empty() ? L"Could not process the WAV file." : message);
        }
        active_.store(false);
    }

    mutable std::mutex mutex_;
    std::thread worker_;
    std::atomic<bool> cancel_{false};
    std::atomic<bool> active_{false};
    std::atomic<float> progress_{0.0F};
    std::wstring status_{L"Ready to process."};
};

WindowsProcessor& processor() {
    static WindowsProcessor instance;
    return instance;
}
}  // namespace

extern "C" AUDIO_STUDIO_API int audio_studio_analyze_wav_waveform(
    const wchar_t* input_path, unsigned int bucket_count, float* interleaved_peaks,
    unsigned long long* total_frames, unsigned int* sample_rate, unsigned short* channels) {
    const std::string input = utf8_path(input_path);
    if (input.empty() || bucket_count == 0U || bucket_count > 1000000U || interleaved_peaks == nullptr) return 0;
    std::vector<audio_studio::WaveformPeak> peaks;
    audio_studio::AudioFormat format{};
    std::uint64_t frames = 0U;
    std::string error;
    if (!audio_studio::analyze_wav_waveform(input, bucket_count, peaks, &format, &frames, &error)) return 0;
    for (std::size_t i = 0; i < peaks.size(); ++i) {
        interleaved_peaks[i * 4U] = peaks[i].min_left;
        interleaved_peaks[i * 4U + 1U] = peaks[i].max_left;
        interleaved_peaks[i * 4U + 2U] = peaks[i].min_right;
        interleaved_peaks[i * 4U + 3U] = peaks[i].max_right;
    }
    if (total_frames) *total_frames = static_cast<unsigned long long>(frames);
    if (sample_rate) *sample_rate = format.sample_rate_hz;
    if (channels) *channels = format.channel_count;
    return static_cast<int>(peaks.size());
}

extern "C" AUDIO_STUDIO_API int audio_studio_processing_start(
    const wchar_t* input_path, const wchar_t* output_path,
    float threshold_dbfs, float attenuation_db, float gain_db, float high_pass_hz) {
    return processor().start(input_path, output_path, threshold_dbfs, attenuation_db,
                             gain_db, high_pass_hz);
}
extern "C" AUDIO_STUDIO_API void audio_studio_processing_cancel(void) {
    processor().cancel();
}
extern "C" AUDIO_STUDIO_API int audio_studio_processing_active(void) {
    return processor().active();
}
extern "C" AUDIO_STUDIO_API float audio_studio_processing_progress(void) {
    return processor().progress();
}
extern "C" AUDIO_STUDIO_API const wchar_t* audio_studio_processing_status(void) {
    return processor().status();
}


extern "C" AUDIO_STUDIO_API int audio_studio_trim_wav(
    const wchar_t* input_path, const wchar_t* output_path,
    unsigned long long start_frame, unsigned long long end_frame) {
    const std::string input = utf8_path(input_path);
    const std::string output = utf8_path(output_path);
    if (input.empty() || output.empty() || start_frame >= end_frame) return 0;
    std::string error;
    return audio_studio::trim_wav(input, output, start_frame, end_frame, &error) ? 1 : 0;
}
