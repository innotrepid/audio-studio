#include "audio_studio/capture_vad_worker.hpp"
#include <cstdlib>
#include <memory>
#include <vector>
using namespace audio_studio;
namespace {
void check(bool ok) { if (!ok) std::exit(1); }
class FakeSession final : public NativeVadSessionInterface {
public:
 bool push_audio(const float* pcm,std::size_t frames,std::vector<SpeechRegion>& out) override {
  check(pcm != nullptr); seen += frames;
  if (emit_on_push) out.push_back({10,40});
  return !fail_push;
 }
 bool flush(std::vector<SpeechRegion>& out) override {
  if (fail_flush) return false;
  out.push_back({50,60}); return true;
 }
 void reset() noexcept override {}
 std::size_t seen{0}; bool emit_on_push{false},fail_push{false},fail_flush{false};
};
}
int main() {
 float pcm[8]={};
 auto fake=std::make_unique<FakeSession>();
 CaptureVadWorker worker(std::move(fake),4,2,2);
 check(worker.submit_audio(pcm,3)==CaptureVadWorker::Status::ready);
 check(worker.queued_frames()==3);
 check(worker.submit_audio(pcm,2)==CaptureVadWorker::Status::backpressure);
 check(worker.finish()==CaptureVadWorker::Status::backpressure);
 check(worker.process_one()==CaptureVadWorker::Status::ready);
 check(worker.queued_frames()==0);
 check(worker.submit_audio(pcm,1)==CaptureVadWorker::Status::ready);
 check(worker.process_one()==CaptureVadWorker::Status::ready);
 check(worker.queued_frames()==0);
 check(worker.finish()==CaptureVadWorker::Status::finished);
 check(worker.finish()==CaptureVadWorker::Status::finished);
 SpeechRegion region{};
 check(worker.pop_region(region) && region.start_ms==50 && region.end_ms==60);
 check(!worker.pop_region(region));
 check(worker.submit_audio(pcm,1)==CaptureVadWorker::Status::invalid);

 auto overflow_session=std::make_unique<FakeSession>();
 overflow_session->emit_on_push=true;
 CaptureVadWorker overflow(std::move(overflow_session),4,1,0);
 check(overflow.submit_audio(pcm,1)==CaptureVadWorker::Status::invalid);

 auto failed_session=std::make_unique<FakeSession>();
 failed_session->fail_push=true;
 CaptureVadWorker failed(std::move(failed_session),4,1);
 check(failed.submit_audio(pcm,1)==CaptureVadWorker::Status::ready);
 check(failed.process_one()==CaptureVadWorker::Status::processing_failed);
 check(failed.failed());
 check(failed.submit_audio(pcm,1)==CaptureVadWorker::Status::invalid);

 auto flush_session=std::make_unique<FakeSession>();
 flush_session->fail_flush=true;
 CaptureVadWorker flush_failed(std::move(flush_session),4,1);
 check(flush_failed.finish()==CaptureVadWorker::Status::processing_failed);
 check(flush_failed.failed());
 return 0;
}