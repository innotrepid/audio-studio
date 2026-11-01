#pragma once

#include "audio_studio/vad.hpp"

#include <memory>
#include <string>

namespace audio_studio {

// Creates a CPU Silero VAD stream from the bundled ONNX model.
// The implementation owns its ONNX session and recurrent state.
// The model path must point to the packaged silero_vad.onnx file.
class OnnxSileroVadStream final : public SileroVadStream {
public:
    static std::unique_ptr<OnnxSileroVadStream> create(
        const std::string& model_path,
        std::string& error_message);

    ~OnnxSileroVadStream() override;

    OnnxSileroVadStream(const OnnxSileroVadStream&) = delete;
    OnnxSileroVadStream& operator=(const OnnxSileroVadStream&) = delete;

    bool process_window(const float* mono_pcm_512,
                       float& speech_probability) override;
    void reset() noexcept override;

private:
    struct Impl;
    explicit OnnxSileroVadStream(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

}  // namespace audio_studio
