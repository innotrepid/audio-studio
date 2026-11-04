cmake_minimum_required(VERSION 3.22)
project(AudioStudio LANGUAGES CXX)

include(CTest)
include(FetchContent)

# Pin whisper.cpp once at the repository root so desktop core, tests, and
# platform native targets all resolve the same real CMake target.
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(WHISPER_CURL OFF CACHE BOOL "" FORCE)
set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
set(GGML_OPENMP OFF CACHE BOOL "" FORCE)
FetchContent_Declare(whisper_cpp
    GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp.git
    GIT_TAG 482279b8a18855ba741d7b88c30fcb3874691f59
    GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(whisper_cpp)

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

# CI builds the pinned RNNoise + DeepFilterNet3 Rust C ABI before configuring CMake.
if(WIN32)
    set(NOISE_CANCELLATION_LIBRARY "${CMAKE_SOURCE_DIR}/target/release/noise_cancellation.lib")
else()
    set(NOISE_CANCELLATION_LIBRARY "${CMAKE_SOURCE_DIR}/target/release/libnoise_cancellation.a")
endif()
add_library(noise_cancellation_static STATIC IMPORTED GLOBAL)
set_target_properties(noise_cancellation_static PROPERTIES
    IMPORTED_LOCATION "${NOISE_CANCELLATION_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/third_party/noise-cancellation"
)
if(WIN32)
    set(NOISE_CANCELLATION_SYSTEM_LIBS "ws2_32;userenv;bcrypt;advapi32;ntdll")
    if(DEFINED DIRECTML_LIBRARY AND EXISTS "${DIRECTML_LIBRARY}")
        list(APPEND NOISE_CANCELLATION_SYSTEM_LIBS "${DIRECTML_LIBRARY}")
    else()
        message(FATAL_ERROR "DeepFilterNet3 Windows build requires DirectML.lib; set DIRECTML_LIBRARY to the x64 library path.")
    endif()
    set_property(TARGET noise_cancellation_static PROPERTY INTERFACE_LINK_LIBRARIES "${NOISE_CANCELLATION_SYSTEM_LIBS}")
elseif(UNIX)
    set_property(TARGET noise_cancellation_static PROPERTY INTERFACE_LINK_LIBRARIES
        "dl;pthread;m;rt;util")
endif()

add_subdirectory(engine)

if(WIN32)
    if(NOT DEFINED ONNXRUNTIME_ROOT OR NOT EXISTS "${ONNXRUNTIME_ROOT}/build/native/include/onnxruntime_cxx_api.h")
        message(FATAL_ERROR "Windows Silero VAD requires ONNXRUNTIME_ROOT pointing to the staged ONNX Runtime 1.23.2 NuGet package.")
    endif()
    set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_ROOT}/build/native/include")
    set(ONNXRUNTIME_LIBRARY "${ONNXRUNTIME_ROOT}/runtimes/win-x64/native/onnxruntime.lib")
    set(ONNXRUNTIME_DLL "${ONNXRUNTIME_ROOT}/runtimes/win-x64/native/onnxruntime.dll")
    if(NOT EXISTS "${ONNXRUNTIME_LIBRARY}" OR NOT EXISTS "${ONNXRUNTIME_DLL}")
        message(FATAL_ERROR "ONNX Runtime 1.23.2 x64 import library or runtime DLL is missing.")
    endif()

    add_library(audio_studio_native SHARED
        engine/src/native_api.cpp
        engine/src/audio_types.cpp
        engine/src/wav_writer.cpp
        engine/src/audio_decoder.cpp
        engine/src/wav_recovery.cpp
        engine/src/noise_gate.cpp
        engine/src/dsp_effects.cpp
        engine/src/silero_onnx.cpp
        engine/src/native_vad_session.cpp
        engine/src/native_vad_api.cpp
        engine/src/audio_resampler.cpp
        engine/src/vad.cpp
        engine/src/wav_processing.cpp
        platforms/windows/native/windows_capture.cpp
        platforms/windows/native/windows_processing.cpp
    )
    target_include_directories(audio_studio_native PRIVATE engine/include platforms/windows/native "${ONNXRUNTIME_INCLUDE_DIR}")
    target_compile_features(audio_studio_native PRIVATE cxx_std_17)
    target_link_libraries(audio_studio_native PRIVATE dsp_engine_static noise_cancellation_static "${ONNXRUNTIME_LIBRARY}" ole32 uuid propsys ws2_32 userenv bcrypt advapi32 ntdll)
    add_custom_command(TARGET audio_studio_native POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNXRUNTIME_DLL}"
            "$<TARGET_FILE_DIR:audio_studio_native>/onnxruntime.dll"
        VERBATIM)
    if(MSVC)
        target_compile_options(audio_studio_native PRIVATE /W4 /WX)
    else()
        target_compile_options(audio_studio_native PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    if(BUILD_TESTING)
        add_executable(audio_studio_silero_onnx_tests
            engine/tests/silero_onnx_test.cpp
            engine/src/native_vad_api.cpp
            engine/src/native_vad_session.cpp
            engine/src/silero_onnx.cpp
            engine/src/audio_resampler.cpp
            engine/src/vad.cpp
        )
        target_include_directories(audio_studio_silero_onnx_tests PRIVATE
            engine/include "${ONNXRUNTIME_INCLUDE_DIR}")
        target_compile_features(audio_studio_silero_onnx_tests PRIVATE cxx_std_17)
        target_link_libraries(audio_studio_silero_onnx_tests PRIVATE "${ONNXRUNTIME_LIBRARY}")
        if(MSVC)
            target_compile_options(audio_studio_silero_onnx_tests PRIVATE /W4 /WX)
        else()
            target_compile_options(audio_studio_silero_onnx_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
        endif()
        add_test(NAME audio_studio_silero_onnx_tests
            COMMAND audio_studio_silero_onnx_tests
                "${CMAKE_BINARY_DIR}/transcription-models/silero_vad.onnx"
                "${CMAKE_BINARY_DIR}/test-data/silero-test.wav")
        set_tests_properties(audio_studio_silero_onnx_tests PROPERTIES
            TIMEOUT 60
            ENVIRONMENT "PATH=${ONNXRUNTIME_ROOT}/runtimes/win-x64/native;$ENV{PATH}")

        # LiveCaptureVad needs production Silero ONNX (Windows ORT package).
        add_executable(audio_studio_live_capture_vad_tests
            engine/tests/live_capture_vad_test.cpp
            engine/src/live_capture_vad.cpp
            engine/src/capture_pcm_ring.cpp
            engine/src/capture_vad_worker.cpp
            engine/src/capture_vad_controller.cpp
            engine/src/native_vad_session.cpp
            engine/src/silero_onnx.cpp
            engine/src/audio_resampler.cpp
            engine/src/vad.cpp
        )
        target_include_directories(audio_studio_live_capture_vad_tests PRIVATE
            engine/include "${ONNXRUNTIME_INCLUDE_DIR}")
        target_compile_features(audio_studio_live_capture_vad_tests PRIVATE cxx_std_17)
        target_link_libraries(audio_studio_live_capture_vad_tests PRIVATE "${ONNXRUNTIME_LIBRARY}")
        if(MSVC)
            target_compile_options(audio_studio_live_capture_vad_tests PRIVATE /W4 /WX)
        else()
            target_compile_options(audio_studio_live_capture_vad_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
        endif()
        add_test(NAME audio_studio_live_capture_vad_tests
            COMMAND audio_studio_live_capture_vad_tests
                "${CMAKE_BINARY_DIR}/transcription-models/silero_vad.onnx")
        set_tests_properties(audio_studio_live_capture_vad_tests PROPERTIES
            TIMEOUT 60
            ENVIRONMENT "PATH=${ONNXRUNTIME_ROOT}/runtimes/win-x64/native;$ENV{PATH}")
    endif()
endif()
