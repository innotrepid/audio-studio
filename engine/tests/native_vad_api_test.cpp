#include "audio_studio/native_api.h"
#include "native_vad_api_test_seam.hpp"
#include <cassert>
#include <memory>
#include <string>
#include <vector>

namespace {
struct Counts { int pushes=0; int flushes=0; int resets=0; };
class FakeSession final : public audio_studio::NativeVadSessionInterface {
public:
 explicit FakeSession(Counts* counts):counts_(counts){}
 bool push_audio(const float*,std::size_t,std::vector<audio_studio::SpeechRegion>& out) override {
  ++counts_->pushes; out={{120,340},{500,760}}; return true;
 }
 bool flush(std::vector<audio_studio::SpeechRegion>& out) override {
  ++counts_->flushes; out={{900,1100},{1200,1450}}; return true;
 }
 void reset() noexcept override { ++counts_->resets; }
private: Counts* counts_;
};
Counts* active_counts=nullptr;
}
namespace audio_studio::native_vad_test {
std::unique_ptr<NativeVadSessionInterface> create_session(const std::string&,std::uint32_t,std::uint32_t,VadOptions,std::string&) {
 return std::make_unique<FakeSession>(active_counts);
}
}
static AudioStudioVadHandle* make_handle(Counts& counts) {
 active_counts=&counts;
 return audio_studio_vad_create(L"fake-model",16000,1,0.5F,100,200,30);
}
static void push_capacity_retry_does_not_reprocess_pcm() {
 Counts counts; auto* h=make_handle(counts); assert(h);
 const float pcm[4]={0,0,0,0}; AudioStudioSpeechRegion out[2]={{1,2},{3,4}}; size_t written=0;
 assert(audio_studio_vad_push_audio(h,pcm,4,out,1,&written)==0);
 assert(written==2); assert(out[0].start_ms==1 && out[0].end_ms==2);
 assert(counts.pushes==1);
 assert(audio_studio_vad_push_audio(h,pcm,4,out,2,&written)==1);
 assert(written==2); assert(out[0].start_ms==120 && out[0].end_ms==340);
 assert(out[1].start_ms==500 && out[1].end_ms==760);
 assert(counts.pushes==1);
 audio_studio_vad_destroy(h);
}
static void flush_capacity_retry_does_not_finalize_twice() {
 Counts counts; auto* h=make_handle(counts); assert(h);
 AudioStudioSpeechRegion out[2]={{7,8},{9,10}}; size_t written=0;
 assert(audio_studio_vad_flush(h,out,1,&written)==0);
 assert(written==2); assert(out[0].start_ms==7 && out[0].end_ms==8);
 assert(counts.flushes==1);
 assert(audio_studio_vad_flush(h,out,2,&written)==1);
 assert(written==2); assert(out[0].start_ms==900 && out[0].end_ms==1100);
 assert(out[1].start_ms==1200 && out[1].end_ms==1450);
 assert(counts.flushes==1);
 audio_studio_vad_destroy(h);
}
static void reset_clears_pending_push_retry() {
 Counts counts; auto* h=make_handle(counts); assert(h);
 const float pcm[4]={0,0,0,0}; AudioStudioSpeechRegion out[2]={{11,12},{13,14}}; size_t written=0;
 assert(audio_studio_vad_push_audio(h,pcm,4,out,1,&written)==0);
 assert(written==2 && counts.pushes==1);
 assert(audio_studio_vad_reset(h)==1);
 assert(counts.resets==1);
 assert(audio_studio_vad_push_audio(h,pcm,4,out,2,&written)==1);
 assert(written==2 && counts.pushes==2);
 assert(out[0].start_ms==120 && out[1].end_ms==760);
 audio_studio_vad_destroy(h);
}
static void c_api_rejects_invalid_arguments_before_session_use() {
 Counts counts; auto* h=make_handle(counts); assert(h);
 const Counts baseline=counts;
 size_t written=99;
 const float pcm[1]={0};
 AudioStudioSpeechRegion out[2]{};
 assert(audio_studio_vad_push_audio(nullptr,pcm,1,out,2,&written)==0);
 assert(written==0);
 assert(audio_studio_vad_push_audio(h,nullptr,1,out,2,&written)==0);
 assert(written==0);
 assert(audio_studio_vad_push_audio(h,pcm,1,out,2,nullptr)==0);
 assert(counts.pushes==baseline.pushes);
 assert(audio_studio_vad_flush(h,out,2,nullptr)==0);
 assert(counts.flushes==baseline.flushes);
 assert(audio_studio_vad_flush(nullptr,out,2,&written)==0);
 assert(written==0);
 assert(audio_studio_vad_reset(nullptr)==0);
 assert(audio_studio_vad_create(nullptr,16000,1,0.5F,100,200,30)==nullptr);
 assert(audio_studio_vad_create(L"fake-model",16000,1,-0.1F,100,200,30)==nullptr);
 assert(audio_studio_vad_create(L"fake-model",16000,1,1.1F,100,200,30)==nullptr);
 assert(counts.pushes==baseline.pushes && counts.flushes==baseline.flushes && counts.resets==baseline.resets);
 audio_studio_vad_destroy(h);
}
static void flush_requires_pending_push_delivery_and_stream_reset_after_flush() {
 Counts counts; auto* h=make_handle(counts); assert(h);
 const float pcm[4]={0,0,0,0}; AudioStudioSpeechRegion out[2]{}; size_t written=0;
 assert(audio_studio_vad_push_audio(h,pcm,4,out,1,&written)==0);
 assert(written==2 && counts.pushes==1);
 assert(audio_studio_vad_flush(h,out,2,&written)==0);
 assert(counts.flushes==0);
 assert(std::string(audio_studio_vad_last_error()).find("retry push_audio")!=std::string::npos);
 assert(audio_studio_vad_push_audio(h,pcm,4,out,2,&written)==1);
 assert(written==2 && counts.pushes==1);
 assert(audio_studio_vad_flush(h,out,2,&written)==1);
 assert(written==2 && counts.flushes==1);
 assert(audio_studio_vad_flush(h,out,2,&written)==1);
 assert(written==0 && counts.flushes==1);
 assert(audio_studio_vad_push_audio(h,pcm,4,out,2,&written)==0);
 assert(counts.pushes==1);
 assert(std::string(audio_studio_vad_last_error()).find("reset before pushing")!=std::string::npos);
 assert(audio_studio_vad_reset(h)==1);
 assert(audio_studio_vad_push_audio(h,pcm,4,out,2,&written)==1);
 assert(counts.pushes==2);
 audio_studio_vad_destroy(h);
}
int main(){push_capacity_retry_does_not_reprocess_pcm();flush_capacity_retry_does_not_finalize_twice();reset_clears_pending_push_retry();flush_requires_pending_push_delivery_and_stream_reset_after_flush();c_api_rejects_invalid_arguments_before_session_use();}
