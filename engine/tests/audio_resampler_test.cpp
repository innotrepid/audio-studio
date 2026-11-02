#include "audio_studio/audio_resampler.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>
namespace { void require(bool v,const char*m){if(!v){std::cerr<<m<<'\n';std::exit(1);}} }
int main(){
 std::vector<float> out;
 require(audio_studio::convert_to_mono(nullptr,0,48000,1,16000,out),"empty input");
 require(out.empty(),"empty output");
 std::vector<float> stereo(960);
 for(std::size_t i=0;i<480;++i){stereo[2*i]=.25f;stereo[2*i+1]=-.25f;}
 require(audio_studio::convert_to_mono(stereo.data(),480,48000,2,16000,out),"stereo conversion");
 require(out.size()==160,"duration preservation");
 for(float x:out)require(std::abs(x)<1e-4f,"mono averaging");
 std::vector<float> bad{0,std::nanf("")};
 require(!audio_studio::convert_to_mono(bad.data(),2,16000,1,16000,out),"reject NaN");
 require(out.empty(),"clear failed output");
 std::vector<float> tone(4800);
 for(std::size_t i=0;i<tone.size();++i)tone[i]=float(.5*std::sin(2*3.141592653589793*1000*i/48000.));
 require(audio_studio::convert_to_mono(tone.data(),tone.size(),48000,1,16000,out),"passband");
 require(out.size()==1600,"10 sec output count");
 double rms=0;for(float x:out)rms+=x*x;rms=std::sqrt(rms/out.size());
 require(rms>.30&&rms<.40,"retain passband");
 std::vector<float> high(4800);
 for(std::size_t i=0;i<high.size();++i)high[i]=float(.5*std::sin(2*3.141592653589793*12000*i/48000.));
 require(audio_studio::convert_to_mono(high.data(),high.size(),48000,1,16000,out),"stopband");
 rms=0;for(float x:out)rms+=x*x;rms=std::sqrt(rms/out.size());
 require(rms<.03,"suppress aliasing");return 0;
}