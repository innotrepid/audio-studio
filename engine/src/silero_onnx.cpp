#include "audio_studio/silero_onnx.hpp"

#include <onnxruntime_cxx_api.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace audio_studio {

struct OnnxSileroVadStream::Impl {
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "Synvidra-Silero-VAD"};
    Ort::SessionOptions options;
    std::unique_ptr<Ort::Session> session;
    std::array<float, kSileroContextSamples> context{};
    std::array<float, 2U * 128U> state{};
    std::array<std::int64_t, 1U> sample_rate{{16000}};
    std::string input_name{"input"};
    std::string state_name{"state"};
    std::string sample_rate_name{"sr"};
    std::string output_name{"output"};
    std::string state_output_name{"stateN"};
};

OnnxSileroVadStream::OnnxSileroVadStream(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

OnnxSileroVadStream::~OnnxSileroVadStream() = default;

std::unique_ptr<OnnxSileroVadStream> OnnxSileroVadStream::create(
    const std::string& model_path, std::string& error_message) {
    error_message.clear();
    try {
        auto impl = std::make_unique<Impl>();
        impl->options.SetIntraOpNumThreads(1);
        impl->options.SetInterOpNumThreads(1);
        impl->options.SetExecutionMode(ORT_SEQUENTIAL);
        impl->session = std::make_unique<Ort::Session>(
            impl->env, model_path.c_str(), impl->options);
        if (impl->session->GetInputCount() != 3U ||
            impl->session->GetOutputCount() != 2U) {
            error_message = "Silero model must expose three inputs and two outputs";
            return nullptr;
        }
        return std::unique_ptr<OnnxSileroVadStream>(
            new OnnxSileroVadStream(std::move(impl)));
    } catch (const Ort::Exception& ex) {
        error_message = ex.what();
    } catch (const std::exception& ex) {
        error_message = ex.what();
    }
    return nullptr;
}

}  // namespace audio_studio
