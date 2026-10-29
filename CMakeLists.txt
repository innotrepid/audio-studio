cmake_minimum_required(VERSION 3.22)
project(AudioStudio LANGUAGES CXX)

include(CTest)
include(FetchContent)

# Pin DSP-ENGINE to an immutable reviewed commit. Build only its static core;
# platform audio backends are intentionally not used by offline WAV processing.
set(DSP_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(DSP_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(DSP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DSP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(DSP_ENABLE_SIMD OFF CACHE BOOL "" FORCE)
set(DSP_BACKEND_AAUDIO OFF CACHE BOOL "" FORCE)
set(DSP_BACKEND_WASAPI OFF CACHE BOOL "" FORCE)
set(DSP_BACKEND_COREAUDIO OFF CACHE BOOL "" FORCE)
set(DSP_BACKEND_ALSA OFF CACHE BOOL "" FORCE)
FetchContent_Declare(dsp_engine_source
    GIT_REPOSITORY https://github.com/innotrepid/DSP-ENGINE.git
    GIT_TAG 05c050c8ebfe3351612cde8cd7d7c94a7d3b1ec3
    GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(dsp_engine_source)

add_subdirectory(engine)

if(WIN32)
    add_library(audio_studio_native SHARED
        engine/src/native_api.cpp
        engine/src/audio_types.cpp
        engine/src/wav_writer.cpp
        engine/src/audio_decoder.cpp
        engine/src/wav_recovery.cpp
        engine/src/noise_gate.cpp
        engine/src/dsp_effects.cpp
        engine/src/wav_processing.cpp
        platforms/windows/native/windows_capture.cpp
        platforms/windows/native/windows_processing.cpp
    )
    target_include_directories(audio_studio_native PRIVATE engine/include platforms/windows/native)
    target_compile_features(audio_studio_native PRIVATE cxx_std_17)
    target_link_libraries(audio_studio_native PRIVATE dsp_engine_static ole32 uuid propsys)
    if(MSVC)
        target_compile_options(audio_studio_native PRIVATE /W4 /WX)
    else()
        target_compile_options(audio_studio_native PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endif()
