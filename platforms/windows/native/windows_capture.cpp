#include "audio_studio/native_api.h"
#include "audio_studio/wav_writer.hpp"
#include "audio_studio/wav_recovery.hpp"
#include "audio_studio/live_capture_vad.hpp"

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
    int start(const wchar_t* path, const wchar_t* silero_model_path = nullptr) {
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
        silero_model_path_.clear();
        if (silero_model_path != nullptr && silero_model_path[0] != L'\0') {
            silero_model_path_ = silero_model_path;
        }
        live_vad_.reset();
        vad_soft_failed_.store(false);
        peak_.store(0.0F);
        clipping_.store(false);
        stop_requested_.store(false);
        pause_requested_.store(false);
        resume_requested_.store(false);
        paused_.store(false);
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

    int pause() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!active_.load() || paused_.load()) return 0;
        pause_requested_.store(true, std::memory_order_release);
        if (audio_event_ != nullptr) SetEvent(audio_event_);
        return pause_cv_.wait_for(lock, std::chrono::seconds(2), [this] {
            return paused_.load() || !active_.load();
        }) && paused_.load() ? 1 : 0;
    }

    int resume() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!active_.load() || !paused_.load()) return 0;
        resume_requested_.store(true, std::memory_order_release);
        if (audio_event_ != nullptr) SetEvent(audio_event_);
        return pause_cv_.wait_for(lock, std::chrono::seconds(2), [this] {
            return !paused_.load() || !active_.load();
        }) && active_.load() && !paused_.load() ? 1 : 0;
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

    std::size_t pending_vad_regions() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return live_vad_ ? live_vad_->pending_region_count() : 0U;
    }

    bool pop_vad_region(audio_studio::SpeechRegion& region) {
        std::lock_guard<std::mutex> lock(mutex_);
        return live_vad_ && live_vad_->pop_region(region);
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

        if (SUCCEEDED(hr)) hr = client_init(enumerator, device, client, capture, mix_format, event_handle, selected_device_id, hr);
        (void)hr;
        // NOTE: body continues below as original run() implementation
        return; // PLACEHOLDER - should not use
    }

    mutable std::mutex mutex_;
};
