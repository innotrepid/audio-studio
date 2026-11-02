#pragma once

#include "audio_studio/vad.hpp"
#include <cstddef>
#include <vector>

namespace audio_studio {

// Copies queued speech regions into a caller-owned buffer. On insufficient
// capacity, reports the required count and leaves the queue untouched so the
// caller can retry without processing the audio again.
bool copy_speech_regions(const std::vector<SpeechRegion>& queued,
                         SpeechRegion* output, std::size_t capacity,
                         std::size_t* written) noexcept;

}  // namespace audio_studio
