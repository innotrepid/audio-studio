#include "audio_studio/vad.hpp"

#include <algorithm>
#include <cmath>

namespace audio_studio {
namespace {

bool valid_options(const VadOptions& options) {
    return std::isfinite(options.speech_threshold) &&
           options.speech_threshold >= 0.0F &&
           options.speech_threshold <= 1.0F &&
           options.minimum_speech_ms > 0U;
}

bool valid_frames(const std::vector<VadProbabilityFrame>& frames) {
    std::uint64_t previous_end = 0U;
    bool first = true;
    for (const auto& frame : frames) {
        if (frame.end_ms <= frame.start_ms ||
            !std::isfinite(frame.speech_probability) ||
            frame.speech_probability < 0.0F ||
            frame.speech_probability > 1.0F ||
            (!first && frame.start_ms < previous_end)) {
            return false;
        }
        previous_end = frame.end_ms;
        first = false;
    }
    return true;
}

}  // namespace

std::vector<SpeechRegion> build_speech_regions(
    const std::vector<VadProbabilityFrame>& frames,
    const VadOptions& options) {
    std::vector<SpeechRegion> regions;
    if (frames.empty() || !valid_options(options) || !valid_frames(frames)) {
        return regions;
    }

    bool active = false;
    std::uint64_t start = 0U;
    std::uint64_t last_speech_end = 0U;

    for (const auto& frame : frames) {
        if (frame.speech_probability >= options.speech_threshold) {
            if (!active) {
                active = true;
                start = frame.start_ms;
            }
            last_speech_end = frame.end_ms;
            continue;
        }

        if (active && frame.end_ms - last_speech_end >= options.minimum_silence_ms) {
            if (last_speech_end - start >= options.minimum_speech_ms) {
                const auto padded_start = start > options.padding_ms
                    ? start - options.padding_ms : 0U;
                regions.push_back({padded_start, last_speech_end + options.padding_ms});
            }
            active = false;
        }
    }

    if (active && last_speech_end - start >= options.minimum_speech_ms) {
        const auto padded_start = start > options.padding_ms
            ? start - options.padding_ms : 0U;
        regions.push_back({padded_start, last_speech_end + options.padding_ms});
    }

    return regions;
}

}  // namespace audio_studio
