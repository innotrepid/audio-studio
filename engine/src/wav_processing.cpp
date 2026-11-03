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

std::uint16_t read_u16_le(const unsigned char* bytes) noexcept {
    return static_cast<std::uint16_t>(bytes[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[1]) << 8U);
}

std::uint32_t read_u32_le(const unsigned char* bytes) noexcept {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

bool read_exact(std::ifstream& stream, unsigned char* destination, std::size_t size) {
    stream.read(reinterpret_cast<char*>(destination), static_cast<std::streamsize>(size));
    return static_cast<bool>(stream);
}

bool report_progress(const std::function<void(std::uint64_t, std::uint64_t)>& progress,
                     std::uint64_t processed, std::uint64_t total,
                     std::string* error) {
    if (!progress) return true;
    try {
        progress(processed, total);
        return true;
    } catch (const std::exception& exception) {
        return fail(error, std::string("Progress callback failed: ") + exception.what());
    } catch (...) {
        return fail(error, "Progress callback failed with an unknown exception.");
    }
}

bool tag_is(const std::array<unsigned char, 4U>& tag, const char* expected) noexcept {
    return tag[0] == static_cast<unsigned char>(expected[0]) &&
           tag[1] == static_cast<unsigned char>(expected[1]) &&
           tag[2] == static_cast<unsigned char>(expected[2]) &&
           tag[3] == static_cast<unsigned char>(expected[3]);
}

// NOTE: This is a partial restore marker - full file follows in next commit if truncated
}  // namespace
}  // namespace audio_studio
