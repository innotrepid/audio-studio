#include "audio_studio/native_vad_session.hpp"
#include "audio_studio/audio_resampler.hpp"
#ifndef AUDIO_STUDIO_NATIVE_VAD_TESTING
#include "audio_studio/silero_onnx.hpp"
#endif
#include <utility>
namespace audio_studio {
struct NativeVadSession::Impl {
 std::uint32_t input_rate;
 std::uint32_t channels;
 std::unique_ptr<SileroVadStream> stream;
 std::unique_ptr<StreamingVadSession> coordinator;
 std::vector<float> converted;
 Impl(std::uint32_t rate,std::uint32_t count,std::unique_ptr<SileroVadStream> s,VadOptions options)
  :input_rate(rate),channels(count),stream(std::move(s)),coordinator(std::make_unique<StreamingVadSession>(*stream,options)){}
};
NativeVadSession::NativeVadSession(std::unique_ptr<Impl> impl):impl_(std::move(impl)){}
NativeVadSession::~NativeVadSession()=default;
#ifndef AUDIO_STUDIO_NATIVE_VAD_TESTING
std::unique_ptr<NativeVadSession> NativeVadSession::create(const std::string& path,
 std::uint32_t rate,std::uint32_t channels,VadOptions options,std::string& error){
 if(rate<8000||rate>192000||channels==0||channels>32){error="Unsupported input audio format.";return nullptr;}
 auto stream=OnnxSileroVadStream::create(path,error);if(!stream)return nullptr;
 try{return std::unique_ptr<NativeVadSession>(new NativeVadSession(std::make_unique<Impl>(rate,channels,std::move(stream),options)));}
 catch(const std::exception& e){error=e.what();return nullptr;}catch(...){error="Could not allocate native VAD session.";return nullptr;}
}
#else
std::unique_ptr<NativeVadSession> NativeVadSession::create_for_testing(std::uint32_t rate,
 std::uint32_t channels,std::unique_ptr<SileroVadStream> stream,VadOptions options){
 if(rate<8000||rate>192000||channels==0||channels>32||!stream)return nullptr;
 try{return std::unique_ptr<NativeVadSession>(new NativeVadSession(std::make_unique<Impl>(rate,channels,std::move(stream),options)));}
 catch(...){return nullptr;}
}
#endif

bool NativeVadSession::push_audio(const float* input,std::size_t frames,std::vector<SpeechRegion>& regions){
 regions.clear();
 if(!impl_ || !impl_->coordinator || !impl_->coordinator->accepting_audio()) return false;
 if(!convert_to_mono(input,frames,impl_->input_rate,impl_->channels,kVadSampleRateHz,impl_->converted))return false;
 return impl_->coordinator->push_audio(impl_->converted.data(),impl_->converted.size(),regions);
}
bool NativeVadSession::flush(std::vector<SpeechRegion>& regions){regions.clear();return impl_&&impl_->coordinator->flush(regions);}
void NativeVadSession::reset() noexcept {if(impl_){impl_->coordinator->reset();impl_->converted.clear();}}
}
