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
        const std::array<const char*, 3U> expected_inputs{{"input", "state", "sr"}};
        const std::array<const char*, 2U> expected_outputs{{"output", "stateN"}};
        Ort::AllocatorWithDefaultOptions allocator;
        for (std::size_t i = 0; i < expected_inputs.size(); ++i) {
            auto name = impl->session->GetInputNameAllocated(i, allocator);
            if (std::string(name.get()) != expected_inputs[i]) {
                error_message = "Unexpected Silero input tensor name";
                return nullptr;
            }
        }
        for (std::size_t i = 0; i < expected_outputs.size(); ++i) {
            auto name = impl->session->GetOutputNameAllocated(i, allocator);
            if (std::string(name.get()) != expected_outputs[i]) {
                error_message = "Unexpected Silero output tensor name";
                return nullptr;
            }
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

bool OnnxSileroVadStream::process_window(const float* mono_pcm_512,
                                         float& speech_probability) {
    if (!impl_ || mono_pcm_512 == nullptr) return false;
    for (std::size_t i = 0; i < kSileroWindowSamples; ++i) {
        if (!std::isfinite(mono_pcm_512[i])) return false;
    }

    std::array<float, kSileroContextSamples + kSileroWindowSamples> input{};
    std::copy(impl_->context.begin(), impl_->context.end(), input.begin());
    std::copy(mono_pcm_512, mono_pcm_512 + kSileroWindowSamples,
              input.begin() + kSileroContextSamples);

    const std::array<std::int64_t, 2U> input_shape{{1, 576}};
    const std::array<std::int64_t, 3U> state_shape{{2, 1, 128}};
    const std::array<std::int64_t, 0U> scalar_shape{};
    auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    auto audio_tensor = Ort::Value::CreateTensor<float>(
        memory, input.data(), input.size(), input_shape.data(), input_shape.size());
    auto state_tensor = Ort::Value::CreateTensor<float>(
        memory, impl_->state.data(), impl_->state.size(),
        state_shape.data(), state_shape.size());
    auto rate_tensor = Ort::Value::CreateTensor<std::int64_t>(
        memory, impl_->sample_rate.data(), impl_->sample_rate.size(),
        scalar_shape.data(), scalar_shape.size());

    const std::array<const char*, 3U> input_names{{
        impl_->input_name.c_str(), impl_->state_name.c_str(),
        impl_->sample_rate_name.c_str()}};
    const std::array<const char*, 2U> output_names{{
        impl_->output_name.c_str(), impl_->state_output_name.c_str()}};
    std::array<Ort::Value, 3U> input_values{{
        std::move(audio_tensor), std::move(state_tensor), std::move(rate_tensor)}};

    try {
        auto outputs = impl_->session->Run(
            Ort::RunOptions{nullptr}, input_names.data(), input_values.data(),
            input_values.size(), output_names.data(), output_names.size());
        if (outputs.size() != 2U || !outputs[0].IsTensor() ||
            !outputs[1].IsTensor()) return false;
        const auto probability_info = outputs[0].GetTensorTypeAndShapeInfo();
        const auto state_info = outputs[1].GetTensorTypeAndShapeInfo();
        if (probability_info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
            state_info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
            probability_info.GetElementCount() != 1U ||
            state_info.GetElementCount() != impl_->state.size()) return false;

        const float probability = outputs[0].GetTensorData<float>()[0];
        if (!std::isfinite(probability) || probability < 0.0F ||
            probability > 1.0F) return false;
        const float* next_state = outputs[1].GetTensorData<float>();
        for (std::size_t i = 0; i < impl_->state.size(); ++i) {
            if (!std::isfinite(next_state[i])) return false;
        }
        std::copy(next_state, next_state + impl_->state.size(), impl_->state.begin());
        std::copy(input.end() - kSileroContextSamples, input.end(),
                  impl_->context.begin());
        speech_probability = probability;
        return true;
    } catch (const Ort::Exception&) {
        return false;
    }
}

void OnnxSileroVadStream::reset() noexcept {
    if (!impl_) return;
    impl_->context.fill(0.0F);
    impl_->state.fill(0.0F);
}

}  // namespace audio_studio
