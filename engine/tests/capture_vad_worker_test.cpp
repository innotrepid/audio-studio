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
  check(pcm != nullptr);
  seen += frames;
  if (emit_on_push) {
   const auto start = static_cast<std::uint64_t>(10U + push_count * 30U);
   out.push_back({start, start + 20});
  }
  ++push_count;
  return !fail_push;
 }
 bool flush(std::vector<SpeechRegion>& out) override {
  if (fail_flush) return false;
  out.push_back({static_cast<std::uint64_t>(10U + push_count * 30U),
                 static_cast<std::uint64_t>(30U + push_count * 30U)});
  return true;
 }
 void reset() noexcept override {}
 std::size_t seen{0};
 std::size_t push_count{0};
 bool emit_on_push{false},fail_push{false},fail_flush{false};
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
 check(worker.pop_region(region) && region.start_ms==70 && region.end_ms==90);
 check(!worker.pop_region(region));
 check(worker.submit_audio(pcm,1)==CaptureVadWorker::Status::invalid);

 auto overflow_session=std::make_unique<FakeSession>();
 overflow_session->emit_on_push=true;
 CaptureVadWorker overflow(std::move(overflow_session),4,1,0);
 check(overflow.submit_audio(pcm,1)==CaptureVadWorker::Status::invalid);

 // Speech regions from separate batches and final flush are delivered once,
 // in production order, with no duplicate after the queue is drained.
 auto delivery_session=std::make_unique<FakeSession>();
 delivery_session->emit_on_push=true;
 CaptureVadWorker delivery(std::move(delivery_session),8,1,3);
 check(delivery.submit_audio(pcm,2)==CaptureVadWorker::Status::ready);
 check(delivery.process_one()==CaptureVadWorker::Status::ready);
 check(delivery.submit_audio(pcm,2)==CaptureVadWorker::Status::ready);
 check(delivery.process_one()==CaptureVadWorker::Status::ready);
 check(delivery.finish()==CaptureVadWorker::Status::finished);
 const SpeechRegion expected[]={{10,30},{40,60},{70,90}};
 for (const auto& item : expected) {
  check(delivery.pop_region(region));
  check(region.start_ms==item.start_ms && region.end_ms==item.end_ms);
 }
 check(!delivery.pop_region(region));

 // A consumer that falls behind must get a deterministic failure rather
 // than silently losing a region when the bounded queue is full.
 auto bounded_session=std::make_unique<FakeSession>();
 bounded_session->emit_on_push=true;
 CaptureVadWorker bounded(std::move(bounded_session),8,1,1);
 check(bounded.submit_audio(pcm,1)==CaptureVadWorker::Status::ready);
 check(bounded.process_one()==CaptureVadWorker::Status::ready);
 check(bounded.submit_audio(pcm,1)==CaptureVadWorker::Status::ready);
 check(bounded.process_one()==CaptureVadWorker::Status::processing_failed);
 check(bounded.failed());

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