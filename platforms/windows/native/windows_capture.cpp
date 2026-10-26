#include "audio_studio/native_api.h"
#include "audio_studio/wav_writer.hpp"

#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <ksmedia.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {
using Microsoft::WRL::ComPtr;

class WindowsCapture final {
public:
    int start(const wchar_t* path) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (worker_.joinable() || path == nullptr || path[0] == L'\0') {
            status_ = L"Recording is already active or the output path is invalid.";
            return 0;
        }
        output_path_ = path;
        stop_requested_.store(false);
        startup_complete_ = false;
        startup_ok_ = false;
        status_ = L"Opening default microphone…";
        worker_ = std::thread(&WindowsCapture::run, this);
        startup_cv_.wait(lock, [this] { return startup_complete_; });
        if (!startup_ok_) {
            lock.unlock();
            if (worker_.joinable()) worker_.join();
            return 0;
        }
        return 1;
    }

    int stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!worker_.joinable()) {
                status_ = L"No recording is active.";
                return 0;
            }
            stop_requested_.store(true);
            if (audio_event_ != nullptr) SetEvent(audio_event_);
        }
        if (worker_.joinable()) worker_.join();
        std::lock_guard<std::mutex> lock(mutex_);
        return status_.find(L"saved") != std::wstring::npos ? 1 : 0;
    }

    int active() const noexcept {
        return active_.load() ? 1 : 0;
    }

    const wchar_t* status() {
        thread_local std::wstring snapshot;
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot = status_;
        return snapshot.c_str();
    }

private:
    void publish_startup(bool okay, const std::wstring& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        startup_ok_ = okay;
        startup_complete_ = true;
        active_.store(okay);
        status_ = message;
        startup_cv_.notify_all();
    }

    void set_status(const std::wstring& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        status_ = message;
    }

    void run() {
        const HRESULT com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const bool uninitialize_com = SUCCEEDED(com_result);
        if (FAILED(com_result) && com_result != RPC_E_CHANGED_MODE) {
            publish_startup(false, L"Could not initialize Windows audio services.");
            return;
        }

        ComPtr<IMMDeviceEnumerator> enumerator;
        ComPtr<IMMDevice> device;
        ComPtr<IAudioClient> client;
        ComPtr<IAudioCaptureClient> capture;
        WAVEFORMATEX* mix_format = nullptr;
        HANDLE event_handle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        HRESULT hr = event_handle == nullptr ? E_FAIL : S_OK;

        if (SUCCEEDED(hr)) hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
            CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
        if (SUCCEEDED(hr)) hr = enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &device);
        if (SUCCEEDED(hr)) hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
            nullptr, reinterpret_cast<void**>(client.GetAddressOf()));
        if (SUCCEEDED(hr)) hr = client->GetMixFormat(&mix_format);

        bool is_float = false;
        bool is_pcm16 = false;
        std::uint16_t channels = 0;
        std::uint32_t sample_rate = 0;
        if (SUCCEEDED(hr) && mix_format != nullptr) {
            channels = mix_format->nChannels;
            sample_rate = mix_format->nSamplesPerSec;
            GUID subtype = mix_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE
                ? reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mix_format)->SubFormat
                : (mix_format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT
                    ? KSDATAFORMAT_SUBTYPE_IEEE_FLOAT : KSDATAFORMAT_SUBTYPE_PCM);
            is_float = subtype == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT &&
                mix_format->wBitsPerSample == 32;
            is_pcm16 = subtype == KSDATAFORMAT_SUBTYPE_PCM &&
                mix_format->wBitsPerSample == 16;
            if ((channels != 1 && channels != 2) || sample_rate < 8000 ||
                sample_rate > 192000 || (!is_float && !is_pcm16)) {
                hr = AUDCLNT_E_UNSUPPORTED_FORMAT;
            }
        }

        if (SUCCEEDED(hr)) hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 1000000, 0, mix_format, nullptr);
        if (SUCCEEDED(hr)) hr = client->SetEventHandle(event_handle);
        if (SUCCEEDED(hr)) hr = client->GetService(IID_PPV_ARGS(&capture));

        audio_event_ = event_handle;
        if (SUCCEEDED(hr)) {
            audio_studio::WavWriter writer;
            if (!writer.open(std::filesystem::path(output_path_),
                    audio_studio::AudioFormat{sample_rate, channels})) {
                hr = E_FAIL;
            } else {
                hr = client->Start();
                if (SUCCEEDED(hr)) {
                    publish_startup(true, L"Recording from the default microphone…");
                    const auto bytes_per_sample = mix_format->wBitsPerSample / 8U;
                    std::vector<float> converted;
                    HRESULT capture_result = S_OK;
                    while (!stop_requested_.load()) {
                        const DWORD wait_result = WaitForSingleObject(event_handle, 200);
                        if (wait_result != WAIT_OBJECT_0) continue;
                        UINT32 packet_frames = 0;
                        capture_result = capture->GetNextPacketSize(&packet_frames);
                        while (SUCCEEDED(capture_result) && packet_frames > 0) {
                            BYTE* data = nullptr;
                            UINT32 frames = 0;
                            DWORD flags = 0;
                            capture_result = capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
                            if (FAILED(capture_result)) break;
                            const auto sample_count = static_cast<std::size_t>(frames) * channels;
                            converted.resize(sample_count);
                            if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0 || data == nullptr) {
                                std::fill(converted.begin(), converted.end(), 0.0F);
                            } else if (is_float) {
                                const auto* input = reinterpret_cast<const float*>(data);
                                std::copy(input, input + sample_count, converted.begin());
                            } else {
                                const auto* input = reinterpret_cast<const std::int16_t*>(data);
                                for (std::size_t i = 0; i < sample_count; ++i) {
                                    converted[i] = static_cast<float>(input[i]) / 32768.0F;
                                }
                            }
                            const bool wrote = writer.write_frames(converted.data(), frames);
                            const HRESULT release_hr = capture->ReleaseBuffer(frames);
                            if (!wrote || FAILED(release_hr)) {
                                capture_result = E_FAIL;
                                break;
                            }
                            capture_result = capture->GetNextPacketSize(&packet_frames);
                        }
                        if (FAILED(capture_result)) {
                            set_status(L"Capture stopped because the audio device or file write failed. Incomplete output was discarded.");
                            break;
                        }
                    }
                    (void)client->Stop();
                    active_.store(false);
                    const bool should_finalize = SUCCEEDED(capture_result);
                    if (should_finalize && writer.finalize()) {
                        set_status(L"Recording saved as a PCM WAV file.");
                    } else {
                        writer.abort();
                        std::error_code ignored;
                        std::filesystem::remove(std::filesystem::path(output_path_), ignored);
                        if (should_finalize) set_status(L"Could not finalize the WAV file; incomplete output was discarded.");
                    }
                    hr = should_finalize ? S_OK : E_FAIL;
                } else {
                    writer.abort();
                    std::error_code ignored;
                    std::filesystem::remove(std::filesystem::path(output_path_), ignored);
                }
            }
        }

        if (mix_format != nullptr) CoTaskMemFree(mix_format);
        audio_event_ = nullptr;
        if (event_handle != nullptr) CloseHandle(event_handle);
        if (uninitialize_com) CoUninitialize();

        bool startup_was_published = false;
        { std::lock_guard<std::mutex> lock(mutex_); startup_was_published = startup_complete_; }
        if (!startup_was_published) {
            const std::wstring message = hr == AUDCLNT_E_UNSUPPORTED_FORMAT
                ? L"Unsupported microphone format. Choose a mono or stereo 16-bit PCM / 32-bit float input."
                : L"Could not start microphone capture. Check Windows microphone privacy and device settings.";
            publish_startup(false, message);
        }
        active_.store(false);
    }

    mutable std::mutex mutex_;
    std::condition_variable startup_cv_;
    std::thread worker_;
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> active_{false};
    bool startup_complete_{false};
    bool startup_ok_{false};
    HANDLE audio_event_{nullptr};
    std::filesystem::path output_path_;
    std::wstring status_{L"Ready."};
};

WindowsCapture& capture_instance() {
    static WindowsCapture instance;
    return instance;
}
}  // namespace

extern "C" AUDIO_STUDIO_API int audio_studio_recording_start(const wchar_t* output_path) {
    return capture_instance().start(output_path);
}
extern "C" AUDIO_STUDIO_API int audio_studio_recording_stop(void) {
    return capture_instance().stop();
}
extern "C" AUDIO_STUDIO_API int audio_studio_recording_active(void) {
    return capture_instance().active();
}
extern "C" AUDIO_STUDIO_API const wchar_t* audio_studio_recording_status(void) {
    return capture_instance().status();
}
