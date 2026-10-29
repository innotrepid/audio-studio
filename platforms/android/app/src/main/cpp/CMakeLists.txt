cmake_minimum_required(VERSION 3.22.1)
project(audio_studio_android LANGUAGES CXX)

include(FetchContent)

# Keep Android on the exact same DSP revision as the desktop builds.
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

add_library(audio_studio_android SHARED native_bridge.cpp)
target_compile_features(audio_studio_android PRIVATE cxx_std_17)

add_library(audio_studio_core STATIC
    ../../../../../../engine/src/audio_types.cpp
    ../../../../../../engine/src/wav_writer.cpp
    ../../../../../../engine/src/wav_reader.cpp
    ../../../../../../engine/src/audio_decoder.cpp
    ../../../../../../engine/src/wav_recovery.cpp
    ../../../../../../engine/src/noise_gate.cpp
    ../../../../../../engine/src/dsp_effects.cpp
    ../../../../../../engine/src/wav_processing.cpp
)
target_include_directories(audio_studio_core PUBLIC
    ../../../../../../engine/include
)
target_compile_features(audio_studio_core PUBLIC cxx_std_17)
target_link_libraries(audio_studio_core PUBLIC dsp_engine_static)

find_package(oboe REQUIRED CONFIG)

target_link_libraries(audio_studio_android
    PRIVATE audio_studio_core oboe::oboe log
)
