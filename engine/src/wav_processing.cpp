#include "audio_studio/wav_processing.hpp"

#include "audio_studio/wav_writer.hpp"
#include "audio_studio/audio_decoder.hpp"
#include "audio_studio/dsp_effects.hpp"
#include "noise_cancellation.h"

#include <algorithm>
#include <atomic>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <exception>
#include <string>
#include <vector>

namespace audio_studio {
namespace {
constexpr std::size_t kFramesPerBlock = 4096U;

bool fail(std::string* error, const std::string& message) {
    if (error != nullptr) *error = message;
    return false;
}

// SEE ARTIFACT FILE - truncated intentionally for tool limit test
}
}
