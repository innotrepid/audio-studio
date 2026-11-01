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

}  // namespace audio_studio
