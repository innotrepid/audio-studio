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
  check(pcm != nullptr); seen += frames; if (seen >= 4) out.push_back({10,40}); return true;
 }
 bool flush(std::vector<SpeechRegion>& out) override { out.push_back({50,60}); return true; }
 void reset() noexcept override {}
 std::size_t seen{0};
};
}
int main() {
 auto fake=std::make_unique<FakeSession>();
 CaptureVadWorker worker(std::move(fake),4,2);
 float pcm[8]={};
 check(worker.submit_audio(pcm,3)==CaptureVadWorker::Status::ready);
 check(worker.queued_frames()==3);
 check(worker.submit_audio(pcm,2)==CaptureVadWorker::Status::backpressure);
 check(worker.process_one()==CaptureVadWorker::Status::ready);
 check(worker.queued_frames()==0);
 check(worker.submit_audio(pcm,1)==CaptureVadWorker::Status::ready);
 check(worker.process_one()==CaptureVadWorker::Status::ready);
 check(worker.queued_frames()==0);
 SpeechRegion region{};
 check(worker.pop_region(region) && region.start_ms==10 && region.end_ms==40);
 check(worker.finish()==CaptureVadWorker::Status::finished);
 check(worker.finish()==CaptureVadWorker::Status::finished);
 check(worker.pop_region(region) && region.start_ms==50 && region.end_ms==60);
 check(!worker.pop_region(region));
 check(worker.submit_audio(pcm,1)==CaptureVadWorker::Status::invalid);
 return 0;
}
