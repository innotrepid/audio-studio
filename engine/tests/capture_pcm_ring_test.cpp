#include "audio_studio/capture_pcm_ring.hpp"
#include <cstdlib>
using namespace audio_studio;
void check(bool value) { if (!value) std::exit(1); }
int main() {
 CapturePcmRing ring(4,2);
 const float input[]={1,2,3,4,5,6};
 check(ring.submit_audio(input,3)==CapturePcmRing::Status::ready);
 check(ring.queued_frames()==3);
 check(ring.submit_audio(input,2)==CapturePcmRing::Status::backpressure);
 float output[8]={};
 check(ring.pop_audio(output,2)==2);
 check(output[0]==1 && output[1]==2 && output[2]==3 && output[3]==4);
 const float wrapped[]={7,8,9,10};
 check(ring.submit_audio(wrapped,2)==CapturePcmRing::Status::ready);
 check(ring.queued_frames()==3);
 check(ring.pop_audio(output,4)==3);
 check(output[0]==5 && output[1]==6 && output[2]==7 && output[3]==8 &&
       output[4]==9 && output[5]==10);
 check(ring.queued_frames()==0);
 ring.close();
 check(ring.submit_audio(input,1)==CapturePcmRing::Status::closed);
 check(ring.pop_audio(nullptr,1)==0);
 CapturePcmRing invalid(4,0);
 check(invalid.submit_audio(input,1)==CapturePcmRing::Status::invalid);
 return 0;
}