#include "audio_studio/vad.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

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

// Clamp padded region ends to the last observed frame end so offline postprocessing
// matches StreamingVadSession::finalize_region.
void push_padded_region(std::vector<SpeechRegion>& regions, std::uint64_t start,
                        std::uint64_t last_speech_end, std::uint64_t stream_end_ms,
                        std::uint64_t padding_ms) {
    const auto padded_start = start > padding_ms ? start - padding_ms : 0U;
    const auto padded_end = last_speech_end > std::numeric_limits<std::uint64_t>::max() - padding_ms
        ? std::numeric_limits<std::uint64_t>::max()
        : last_speech_end + padding_ms;
    const auto end = std::min(stream_end_ms, padded_end);
    if (end > padded_start) {
        regions.push_back({padded_start, end});
    }
}

}  // namespace

std::vector<SpeechRegion> build_speech_regions(
    const std::vector<VadProbabilityFrame>& frames,
    const VadOptions& options) {
    std::vector<SpeechRegion> regions;
    if (frames.empty() || !valid_options(options) || !valid_frames(frames)) {
        return regions;
    }

    const std::uint64_t stream_end_ms = frames.back().end_ms;
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
                push_padded_region(regions, start, last_speech_end, stream_end_ms,
                                   options.padding_ms);
            }
            active = false;
        }
    }

    if (active && last_speech_end - start >= options.minimum_speech_ms) {
        push_padded_region(regions, start, last_speech_end, stream_end_ms,
                           options.padding_ms);
    }

    return regions;
}


StreamingVadSession::StreamingVadSession(SileroVadStream& stream, VadOptions options)
    : stream_(stream), options_(options) {
    if (!valid_options(options_) || options_.minimum_silence_ms == 0U) flushed_ = true;
}
bool StreamingVadSession::push_audio(const float* pcm, std::size_t count,
                                     std::vector<SpeechRegion>& out) {
    out.clear();
    if (flushed_ || failed_ || (count && !pcm)) return false;
    for (std::size_t i=0;i<count;++i) if (!std::isfinite(pcm[i])) return false;
    if (count > 0U) pending_.insert(pending_.end(), pcm, pcm+count);
    while (pending_.size() >= kSileroWindowSamples) {
        if (!analyze_window(kSileroWindowSamples,out)) {
            failed_ = true;
            out.clear();
            return false;
        }
    }
    return true;
}
bool StreamingVadSession::analyze_window(std::size_t valid,
                                         std::vector<SpeechRegion>& out) {
    if (!valid || valid>kSileroWindowSamples || pending_.size()<valid) return false;
    float window[kSileroWindowSamples]{};
    std::copy_n(pending_.begin(),valid,window);
    float probability=0.0F;
    if (!stream_.process_window(window,probability) || !std::isfinite(probability) ||
        probability<0.0F || probability>1.0F) return false;
    const auto start=samples_seen_;
    samples_seen_+=valid;
    pending_.erase(pending_.begin(),pending_.begin()+static_cast<std::ptrdiff_t>(valid));
    consume_probability(probability,start*1000U/kVadSampleRateHz,
                       samples_seen_*1000U/kVadSampleRateHz,out);
    return true;
}
void StreamingVadSession::consume_probability(float p,std::uint64_t start,
    std::uint64_t end,std::vector<SpeechRegion>& out) {
    if (p>=options_.speech_threshold) {
        if (!active_) { active_=true; speech_start_ms_=start; }
        last_speech_end_ms_=end;
    } else if (active_ && end-last_speech_end_ms_>=options_.minimum_silence_ms) {
        finalize_region(end,out);
    }
}
void StreamingVadSession::finalize_region(std::uint64_t observed,
                                           std::vector<SpeechRegion>& out) {
    if (active_ && last_speech_end_ms_-speech_start_ms_>=options_.minimum_speech_ms) {
        const auto start=speech_start_ms_>options_.padding_ms
            ? speech_start_ms_-options_.padding_ms : 0U;
        const auto padded_end = last_speech_end_ms_ > std::numeric_limits<std::uint64_t>::max() - options_.padding_ms
            ? std::numeric_limits<std::uint64_t>::max() : last_speech_end_ms_ + options_.padding_ms;
        const auto end=std::min(observed,padded_end);
        if (end>start) out.push_back({start,end});
    }
    active_=false; speech_start_ms_=0U; last_speech_end_ms_=0U;
}
bool StreamingVadSession::flush(std::vector<SpeechRegion>& out) {
    out.clear();
    if (flushed_ || failed_) return false;
    if (!pending_.empty() && !analyze_window(pending_.size(),out)) {
        failed_ = true;
        out.clear();
        return false;
    }
    finalize_region(audio_duration_ms(),out);
    flushed_=true;
    return true;
}
void StreamingVadSession::reset() noexcept {
    pending_.clear(); samples_seen_=0U; active_=false;
    speech_start_ms_=0U; last_speech_end_ms_=0U; flushed_=false; failed_=false; stream_.reset();
}
std::uint64_t StreamingVadSession::audio_duration_ms() const noexcept {
    return samples_seen_*1000U/kVadSampleRateHz;
}

}  // namespace audio_studio
