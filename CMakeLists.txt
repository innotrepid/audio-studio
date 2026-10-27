cmake_minimum_required(VERSION 3.22)
project(AudioStudio LANGUAGES CXX)

include(CTest)
add_subdirectory(engine)

if(WIN32)
    add_library(audio_studio_native SHARED
        engine/src/native_api.cpp
        engine/src/audio_types.cpp
        engine/src/wav_writer.cpp
        platforms/windows/native/windows_capture.cpp
        platforms/windows/native/windows_processing.cpp
    )
    target_include_directories(audio_studio_native PRIVATE engine/include platforms/windows/native)
    target_compile_features(audio_studio_native PRIVATE cxx_std_17)
    target_link_libraries(audio_studio_native PRIVATE ole32 uuid)
    if(MSVC)
        target_compile_options(audio_studio_native PRIVATE /W4 /WX)
    else()
        target_compile_options(audio_studio_native PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endif()
