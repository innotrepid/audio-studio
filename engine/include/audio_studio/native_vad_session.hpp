#pragma once
#include "audio_studio/vad.hpp"
#include <memory>
#include <string>
#include <vector>
namespace audio_studio {
// Owns a Silero ONNX stream and the ONNX-independent incremental coordinator.
// Input is interleaved float PCM in the capture device's native rate/channels.
class NativeVadSession {
public:
 static std::unique_ptr<NativeVadSession> create(const std::string& model_path,
  std::uint32_t input_sample_rate,std::uint32_t input_channels,
  VadOptions options,std::string& error_message);
 ~NativeVadSession();
 NativeVadSession(const NativeVadSession&)=delete;
 NativeVadSession& operator=(const NativeVadSession&)=delete;
 bool push_audio(const float* interleaved,std::size_t frame_count,
  std::vector<SpeechRegion>& completed_regions);
 bool flush(std::vector<SpeechRegion>& completed_regions);
 void reset() noexcept;
private:
 struct Impl;
 explicit NativeVadSession(std::unique_ptr<Impl> impl);
 std::unique_ptr<Impl> impl_;
};
}