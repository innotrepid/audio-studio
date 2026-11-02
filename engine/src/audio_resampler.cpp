#include "audio_studio/audio_resampler.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace audio_studio {
namespace { constexpr double kPi=3.14159265358979323846; constexpr int kRadius=16;
bool valid_rate(std::uint32_t r) noexcept { return r>=8000 && r<=192000; } }
bool convert_to_mono(const float* input,std::size_t frames,std::uint32_t in_rate,
 std::uint32_t channels,std::uint32_t out_rate,std::vector<float>& output) noexcept {
 output.clear();
 if(!valid_rate(in_rate)||!valid_rate(out_rate)||channels==0||channels>32||
 (frames&&input==nullptr)||frames>std::numeric_limits<std::size_t>::max()/channels) return false;
 for(std::size_t i=0;i<frames*channels;++i)
  if(!std::isfinite(input[i])||input[i]<-1.0f||input[i]>1.0f) return false;
 if(!frames) return true;
 try {
  std::vector<double> mono(frames);
  for(std::size_t f=0;f<frames;++f){ double sum=0; for(std::uint32_t c=0;c<channels;++c) sum+=input[f*channels+c]; mono[f]=sum/channels; }
  long double exact=static_cast<long double>(frames)*out_rate/in_rate;
  if(exact>static_cast<long double>(std::numeric_limits<std::size_t>::max())) return false;
  auto count=static_cast<std::size_t>(std::llround(exact)); output.reserve(count);
  double ratio=static_cast<double>(out_rate)/in_rate, cutoff=std::min(1.0,ratio)*0.94;
  for(std::size_t oi=0;oi<count;++oi){
   double pos=static_cast<double>(oi)/ratio; auto center=static_cast<std::int64_t>(std::floor(pos));
   double sum=0, norm=0;
   for(int off=-kRadius+1;off<=kRadius;++off){
    auto si=center+off; if(si<0||si>=static_cast<std::int64_t>(frames)) continue;
    double d=pos-static_cast<double>(si), x=d/kRadius; if(std::abs(x)>=1) continue;
    double window=0.5+0.5*std::cos(kPi*x), arg=kPi*cutoff*d;
    double sinc=std::abs(arg)<1e-12?cutoff:cutoff*std::sin(arg)/arg;
    double w=sinc*window; sum+=mono[static_cast<std::size_t>(si)]*w; norm+=w;
   }
   double sample=std::abs(norm)>1e-12?sum/norm:0;
   output.push_back(static_cast<float>(std::clamp(sample,-1.0,1.0)));
  }
 } catch(...) { output.clear(); return false; }
 return true;
}
}