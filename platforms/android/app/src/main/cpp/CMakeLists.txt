cmake_minimum_required(VERSION 3.22.1)
project(audio_studio_android LANGUAGES CXX)

add_library(audio_studio_android SHARED native_bridge.cpp)
target_compile_features(audio_studio_android PRIVATE cxx_std_17)

add_library(audio_studio_core STATIC
    ../../../../../../engine/src/audio_types.cpp
    ../../../../../../engine/src/wav_writer.cpp
    ../../../../../../engine/src/wav_reader.cpp
    ../../../../../../engine/src/noise_gate.cpp
)
target_include_directories(audio_studio_core PUBLIC
    ../../../../../../engine/include
)
target_compile_features(audio_studio_core PUBLIC cxx_std_17)

find_package(oboe REQUIRED CONFIG)

target_link_libraries(audio_studio_android
    PRIVATE audio_studio_core oboe::oboe log
)
