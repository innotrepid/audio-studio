#pragma once

#include <filesystem>
#include <string>

namespace audio_studio {

// Recovers a writer-owned ".partial-N" WAV after an unclean shutdown.
// The destination must be the original filename (the source path with
// ".partial-N" removed). Invalid or unaligned audio is left untouched.
[[nodiscard]] bool recover_partial_wav(const std::filesystem::path& partial_path,
                                       const std::filesystem::path& output_path,
                                       std::string* error = nullptr);

}  // namespace audio_studio
