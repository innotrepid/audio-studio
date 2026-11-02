#include "audio_studio/speech_region_delivery.hpp"

namespace audio_studio {

bool copy_speech_regions(const std::vector<SpeechRegion>& queued,
                         SpeechRegion* output, std::size_t capacity,
                         std::size_t* written) noexcept {
    if (!written) return false;
    *written = queued.size();
    if (queued.size() > capacity || (!queued.empty() && !output)) return false;
    for (std::size_t i = 0; i < queued.size(); ++i) output[i] = queued[i];
    return true;
}

}  // namespace audio_studio
