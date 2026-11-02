#include "audio_studio/wav_audio_reader.hpp"
#include "audio_studio/audio_decoder.hpp"
#include "audio_studio/audio_resampler.hpp"
#include "audio_studio/transcription.hpp"
#include <algorithm>
#include <limits>
#include <memory>
#include <vector>
namespace audio_studio {
bool load_wav_for_transcription(const std::string& path, std::vector<float>& output,
                                std::string* error, std::uint64_t max_samples) {
 output.clear(); if(error) error->clear();
 if(path.empty() || max_samples==0) { if(error)*error="A WAV path and positive output limit are required."; return false; }
 try {
  auto decoder=open_audio_decoder(path,error); if(!decoder)return false;
  const auto fmt=decoder->format();
  const long double estimate=static_cast<long double>(decoder->total_frames())*
    kTranscriptionSampleRateHz/fmt.sample_rate_hz;
  if(estimate>static_cast<long double>(max_samples) ||
     estimate>static_cast<long double>(std::numeric_limits<std::size_t>::max())) {
    if(error)*error="Audio exceeds the configured transcription duration limit."; return false;
  }
  output.reserve(static_cast<std::size_t>(estimate));
  constexpr std::size_t block=4096;
  std::vector<float> interleaved(block*fmt.channel_count), converted;
  while(decoder->position_frames()<decoder->total_frames()) {
   std::size_t read=0;
   if(!decoder->read_frames(interleaved.data(),block,read,error)) {output.clear();return false;}
   if(read==0)break;
   if(!convert_to_mono(interleaved.data(),read,fmt.sample_rate_hz,fmt.channel_count,
                       kTranscriptionSampleRateHz,converted)) {
    output.clear(); if(error)*error="Audio downmix/resampling failed."; return false;
   }
   if(converted.size()>max_samples-output.size()) {
    output.clear();if(error)*error="Audio exceeds the configured transcription duration limit.";return false;
   }
   output.insert(output.end(),converted.begin(),converted.end());
  }
  if(output.empty()){if(error)*error="WAV contains no decodable audio.";return false;}
  return true;
 } catch(...) {output.clear();if(error)*error="Could not allocate transcription audio.";return false;}
}
} // namespace audio_studio
