#pragma once

#include "audio_studio/audio_types.hpp"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>

namespace audio_studio {

// Writes interleaved normalized float audio to a standard little-endian PCM16 WAV.
// All file I/O is performed by the caller's worker thread, never an audio callback.
class WavWriter final {
public:
    WavWriter() = default;
    ~WavWriter();

    WavWriter(const WavWriter&) = delete;
    WavWriter& operator=(const WavWriter&) = delete;

    [[nodiscard]] bool open(const std::string& path, AudioFormat format);
    [[nodiscard]] bool write_frames(const float* interleaved_samples,
                                    std::size_t frame_count);
    [[nodiscard]] bool finalize();
    void abort() noexcept;

    [[nodiscard]] bool is_open() const noexcept { return stream_.is_open(); }
    [[nodiscard]] std::uint64_t frames_written() const noexcept { return frames_written_; }
    [[nodiscard]] const std::string& last_error() const noexcept { return last_error_; }

private:
    void set_error(const char* message);
    std::ofstream stream_{};
    AudioFormat format_{};
    std::uint64_t frames_written_{0};
    std::string last_error_{};
    bool finalized_{false};
};

}  // namespace audio_studio
