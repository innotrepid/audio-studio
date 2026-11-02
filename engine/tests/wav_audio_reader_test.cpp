#include "audio_studio/wav_audio_reader.hpp"
#include "audio_studio/wav_writer.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
namespace { void req(bool v,const char*m){if(!v){std::cerr<<"FAILED: "<<m<<'\n';std::exit(EXIT_FAILURE);}}
void test_mono_16k(){const std::string p="transcription_mono.wav";std::array<float,160> x{};for(size_t i=0;i<x.size();++i)x[i]=0.25F;
{audio_studio::WavWriter w;req(w.open(p,{16000,1}),"create mono fixture");req(w.write_frames(x.data(),x.size()),"write mono");req(w.finalize(),"finalize mono");}
std::vector<float> out;std::string e;req(audio_studio::load_wav_for_transcription(p,out,&e),"decode mono WAV");req(out.size()==160,"preserve 16 kHz sample count");req(out[80]>0.24F,"preserve normalized sample");req(!audio_studio::load_wav_for_transcription(p,out,&e,100),"reject over-limit WAV");req(out.empty(),"clear output on failure");std::remove(p.c_str());}
void test_stereo_downmix_resample(){const std::string p="transcription_stereo.wav";std::array<float,960> x{};for(size_t i=0;i<480;++i){x[2*i]=0.5F;x[2*i+1]=-0.25F;}
{audio_studio::WavWriter w;req(w.open(p,{48000,2}),"create stereo fixture");req(w.write_frames(x.data(),480),"write stereo");req(w.finalize(),"finalize stereo");}
std::vector<float> out;std::string e;req(audio_studio::load_wav_for_transcription(p,out,&e),"decode stereo WAV");req(out.size()==160,"resample 48k to 16k");req(out[80]>0.10F&&out[80]<0.14F,"average stereo channels");std::remove(p.c_str());}
}
int main(){test_mono_16k();test_stereo_downmix_resample();std::cout<<"WAV transcription loader tests passed.\n";}
