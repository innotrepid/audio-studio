#include "audio_studio/native_api.h"
#include "audio_studio/wav_writer.hpp"

#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propsys.h>
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

struct CaptureDeviceInfo {
    std::wstring id;
    std::wstring name;
};

std::vector<CaptureDeviceInfo> enumerate_capture_devices() {
    std::vector<CaptureDeviceInfo> devices;
    const HRESULT com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitialize_com = SUCCEEDED(com_result);
    if (FAILED(com_result) && com_result != RPC_E_CHANGED_MODE) return devices;

    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDeviceCollection> collection;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
        CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
    if (SUCCEEDED(hr)) hr = enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection);
    UINT count = 0;
    if (SUCCEEDED(hr)) hr = collection->GetCount(&count);
    for (UINT i = 0; SUCCEEDED(hr) && i < count; ++i) {
        ComPtr<IMMDevice> device;
        if (FAILED(collection->Item(i, &device))) continue;
        LPWSTR id = nullptr;
        if (FAILED(device->GetId(&id)) || id == nullptr) continue;
        CaptureDeviceInfo info;
        info.id = id;
        CoTaskMemFree(id);
        ComPtr<IPropertyStore> properties;
        if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &properties))) {
            PROPVARIANT value;
            PropVariantInit(&value);
            if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &value)) &&
                value.vt == VT_LPWSTR && value.pwszVal != nullptr) info.name = value.pwszVal;
            PropVariantClear(&value);
        }
        if (info.name.empty()) info.name = info.id;
        devices.push_back(std::move(info));
    }
    if (uninitialize_com) CoUninitialize();
    return devices;
}

std::vector<CaptureDeviceInfo>& capture_device_cache() {
    thread_local std::vector<CaptureDeviceInfo> devices;
    return devices;
}

class WindowsCapture final {
public:
    int start(const wchar_t* path) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (worker_.joinable()) {
            if (active_.load()) {
                status_ = L"Recording is already active.";
                return 0;
            }
            lock.unlock();
            worker_.join();
            lock.lock();
        }
        if (path == nullptr || path[0] == L'\0') {
            status_ = L"The output path is invalid.";
            return 0;
        }
        output_path_ = path;
        peak_.store(0.0F);
        clipping_.store(false);
        stop_requested_.store(false);
        startup_complete_ = false;
        startup_ok_ = false;
        status_ = selected_device_id_.empty()
            ? L"Opening the Windows default microphone…"
            : L"Opening the selected microphone…";
        worker_ = std::thread(&WindowsCapture::run, this, selected_device_id_);
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

    int select_device(const wchar_t* device_id) {
        if (device_id == nullptr) return 0;
        std::lock_guard<std::mutex> lock(mutex_);
        if (active_.load() || !startup_complete_) return 0;
        selected_device_id_ = device_id;
        return 1;
    }

    int active() const noexcept {
        return active_.load() ? 1 : 0;
    }

    float peak() const noexcept {
        return peak_.load();
    }

    int clipping() const noexcept {
        return clipping_.load() ? 1 : 0;
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

    void run(const std::wstring& selected_device_id) {
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
        if (SUCCEEDED(hr)) {
            hr = selected_device_id.empty()
                ? enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &device)
                : enumerator->GetDevice(selected_device_id.c_str(), &device);
        }
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

        { std::lock_guard<std::mutex> lock(mutex_); audio_event_ = event_handle; }
        bool output_open_failed = false;
        if (SUCCEEDED(hr)) {
            audio_studio::WavWriter writer;
            if (!writer.open_path(std::filesystem::path(output_path_),
                    audio_studio::AudioFormat{sample_rate, channels})) {
                hr = E_FAIL;
                output_open_failed = true;
                std::error_code ignored;
                std::filesystem::remove(std::filesystem::path(output_path_), ignored);
            } else {
                hr = client->Start();
                if (SUCCEEDED(hr)) {
                    publish_startup(true, selected_device_id.empty()
                        ? L"Recording from the Windows default microphone…"
                        : L"Recording from the selected microphone…");
                    std::vector<float> converted;
                    HRESULT capture_result = S_OK;
                    while (!stop_requested_.load()) {
                        const DWORD wait_result = WaitForSingleObject(event_handle, 200);
                        if (wait_result == WAIT_TIMEOUT) continue;
                        if (wait_result != WAIT_OBJECT_0) {
                            capture_result = E_FAIL;
                            set_status(L"Microphone capture stopped because the audio event failed. Incomplete output was discarded.");
                            break;
                        }
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
                            float packet_peak = 0.0F;
                            for (const float sample : converted) {
                                const float magnitude = sample < 0.0F ? -sample : sample;
                                packet_peak = (std::max)(packet_peak, magnitude);
                                if (magnitude >= 0.999F) clipping_.store(true);
                            }
                            peak_.store(packet_peak);
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
                    set_status(L"Could not start microphone capture. The incomplete output was discarded.");
                }
            }
        }

        if (mix_format != nullptr) CoTaskMemFree(mix_format);
        { std::lock_guard<std::mutex> lock(mutex_); audio_event_ = nullptr; }
        if (event_handle != nullptr) CloseHandle(event_handle);
        if (uninitialize_com) CoUninitialize();

        bool startup_was_published = false;
        { std::lock_guard<std::mutex> lock(mutex_); startup_was_published = startup_complete_; }
        if (!startup_was_published) {
            std::wstring message;
            if (output_open_failed) {
                message = L"Could not create the output WAV file. Check the destination folder and file permissions.";
            } else if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT) {
                message = L"Unsupported microphone format. Choose a mono or stereo 16-bit PCM / 32-bit float input.";
            } else if (hr == E_ACCESSDENIED) {
                message = L"Windows denied microphone access. Enable microphone access for desktop apps in Windows Privacy settings.";
            } else if (hr == AUDCLNT_E_DEVICE_INVALIDATED || hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) {
                message = selected_device_id.empty()
                    ? L"The default microphone became unavailable. Reconnect it or choose another input."
                    : L"The selected microphone is no longer available. Refresh the input list and choose another device.";
            } else if (hr == AUDCLNT_E_DEVICE_IN_USE) {
                message = L"The microphone could not be opened because it may be in use exclusively by another app. Close that app or change the device audio settings.";
            } else {
                message = L"Could not start microphone capture. Check the selected device, Windows microphone privacy, and audio settings.";
            }
            publish_startup(false, message);
        }
        active_.store(false);
    }

    mutable std::mutex mutex_;
    std::condition_variable startup_cv_;
    std::thread worker_;
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> active_{false};
    std::atomic<float> peak_{0.0F};
    std::atomic<bool> clipping_{false};
    bool startup_complete_{true};
    bool startup_ok_{false};
    HANDLE audio_event_{nullptr};
    std::filesystem::path output_path_;
    std::wstring selected_device_id_;
    std::wstring status_{L"Ready."};
};

WindowsCapture& capture_instance() {
    static WindowsCapture instance;
    return instance;
}
}  // namespace

extern "C" AUDIO_STUDIO_API int audio_studio_capture_device_count(void) {
    capture_device_cache() = enumerate_capture_devices();
    return static_cast<int>(capture_device_cache().size());
}
extern "C" AUDIO_STUDIO_API const wchar_t* audio_studio_capture_device_name(int index) {
    thread_local std::wstring value;
    const auto& devices = capture_device_cache();
    value = index >= 0 && static_cast<std::size_t>(index) < devices.size()
        ? devices[static_cast<std::size_t>(index)].name : L"";
    return value.c_str();
}
extern "C" AUDIO_STUDIO_API const wchar_t* audio_studio_capture_device_id(int index) {
    thread_local std::wstring value;
    const auto& devices = capture_device_cache();
    value = index >= 0 && static_cast<std::size_t>(index) < devices.size()
        ? devices[static_cast<std::size_t>(index)].id : L"";
    return value.c_str();
}
extern "C" AUDIO_STUDIO_API int audio_studio_capture_device_select(const wchar_t* device_id) {
    return capture_instance().select_device(device_id);
}
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
extern "C" AUDIO_STUDIO_API float audio_studio_recording_peak(void) {
    return capture_instance().peak();
}
extern "C" AUDIO_STUDIO_API int audio_studio_recording_clipping(void) {
    return capture_instance().clipping();
}
