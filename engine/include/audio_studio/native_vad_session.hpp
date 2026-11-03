#pragma once
#include "audio_studio/vad.hpp"
#include <memory>
#include <string>
#include <vector>
namespace audio_studio {
// Owns a Silero ONNX stream and the ONNX-independent incremental coordinator.
// Input is interleaved float PCM in the capture device's native rate/channels.
class NativeVadSessionInterface {
public:
 virtual ~NativeVadSessionInterface() = default;
 virtual bool push_audio(const float* interleaved,std::size_t frame_count,
  std::vector<SpeechRegion>& completed_regions) = 0;
 virtual bool flush(std::vector<SpeechRegion>& completed_regions) = 0;
 virtual void reset() noexcept = 0;
};
// Production implementation backed by Silero ONNX.
class NativeVadSession : public NativeVadSessionInterface {
public:
 static std::unique_ptr<NativeVadSession> create(const std::string& model_path,
  std::uint32_t input_sample_rate,std::uint32_t input_channels,
  VadOptions options,std::string& error_message);
 ~NativeVadSession() override;
#ifdef AUDIO_STUDIO_NATIVE_VAD_TESTING
 static std::unique_ptr<NativeVadSession> create_for_testing(std::uint32_t input_sample_rate,
  std::uint32_t input_channels,std::unique_ptr<SileroVadStream> stream,VadOptions options = {});
#endif
 NativeVadSession(const NativeVadSession&)=delete;
 NativeVadSession& operator=(const NativeVadSession&)=delete;
 bool push_audio(const float* interleaved,std::size_t frame_count,
  std::vector<SpeechRegion>& completed_regions) override;
 bool flush(std::vector<SpeechRegion>& completed_regions) override;
 void reset() noexcept override;
private:
 struct Impl;
 explicit NativeVadSession(std::unique_ptr<Impl> impl);
 std::unique_ptr<Impl> impl_;
};
}