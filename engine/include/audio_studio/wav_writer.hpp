#pragma once

#include "audio_studio/audio_types.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
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
    [[nodiscard]] bool open_path(const std::filesystem::path& path, AudioFormat format);
    [[nodiscard]] bool write_frames(const float* interleaved_samples,
                                    std::size_t frame_count);
    [[nodiscard]] bool finalize();
    // Close the stream but keep the temporary file for explicit recovery after a capture failure.
    void preserve_partial() noexcept;
    void abort() noexcept;

    [[nodiscard]] bool is_open() const noexcept { return stream_.is_open(); }
    [[nodiscard]] std::uint64_t frames_written() const noexcept { return frames_written_; }
    [[nodiscard]] const std::string& last_error() const noexcept { return last_error_; }
    [[nodiscard]] std::string partial_path() const { return temporary_path_.string(); }

private:
    void set_error(const char* message);
    void set_error(std::string message);
    std::ofstream stream_{};
    std::filesystem::path output_path_{};
    std::filesystem::path temporary_path_{};
    AudioFormat format_{};
    std::uint64_t frames_written_{0};
    std::string last_error_{};
    bool finalized_{false};
};

}  // namespace audio_studio
