#pragma once
#include "audio_studio/native_vad_session.hpp"
namespace audio_studio::native_vad_test {
std::unique_ptr<NativeVadSessionInterface> create_session(const std::string&, std::uint32_t, std::uint32_t, VadOptions, std::string&);
}
