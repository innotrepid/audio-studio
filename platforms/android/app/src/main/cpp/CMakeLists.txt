cmake_minimum_required(VERSION 3.22.1)
project(audio_studio_android LANGUAGES CXX)

include(FetchContent)

# Keep Android on the exact same DSP revision as the desktop builds.
# Pinned whisper.cpp CPU inference runtime; no network/model access at runtime.
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
set(GGML_OPENMP OFF CACHE BOOL "" FORCE)
FetchContent_Declare(whisper_cpp
    GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp.git
    GIT_TAG 482279b8a18855ba741d7b88c30fcb3874691f59
    GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(whisper_cpp)


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

# Rust RNNoise static library is built for each Android ABI before Gradle/CMake.
if(ANDROID_ABI STREQUAL "arm64-v8a")
    set(NOISE_CANCELLATION_RUST_TARGET "aarch64-linux-android")
elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
    set(NOISE_CANCELLATION_RUST_TARGET "armv7-linux-androideabi")
elseif(ANDROID_ABI STREQUAL "x86")
    set(NOISE_CANCELLATION_RUST_TARGET "i686-linux-android")
elseif(ANDROID_ABI STREQUAL "x86_64")
    set(NOISE_CANCELLATION_RUST_TARGET "x86_64-linux-android")
else()
    message(FATAL_ERROR "Unsupported Android ABI for RNNoise: ${ANDROID_ABI}")
endif()
add_library(noise_cancellation_static STATIC IMPORTED GLOBAL)
set_target_properties(noise_cancellation_static PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/../../../../../../target/${NOISE_CANCELLATION_RUST_TARGET}/release/libnoise_cancellation.a"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/../../../../../../third_party/noise-cancellation"
)

# ONNX Runtime native bits extracted from the pinned onnxruntime-android AAR.
if(NOT DEFINED ONNXRUNTIME_ROOT)
    message(FATAL_ERROR "ONNXRUNTIME_ROOT must point at the extracted onnxruntime-android package")
endif()
set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_ROOT}/headers")
set(ONNXRUNTIME_JNI_LIB "${ONNXRUNTIME_ROOT}/jni/${ANDROID_ABI}/libonnxruntime.so")
if(NOT EXISTS "${ONNXRUNTIME_INCLUDE_DIR}/onnxruntime_cxx_api.h")
    message(FATAL_ERROR "ONNX Runtime C++ headers missing under ${ONNXRUNTIME_INCLUDE_DIR}")
endif()
if(NOT EXISTS "${ONNXRUNTIME_JNI_LIB}")
    message(FATAL_ERROR "ONNX Runtime shared library missing for ${ANDROID_ABI}: ${ONNXRUNTIME_JNI_LIB}")
endif()
add_library(onnxruntime SHARED IMPORTED)
set_target_properties(onnxruntime PROPERTIES
    IMPORTED_LOCATION "${ONNXRUNTIME_JNI_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_INCLUDE_DIR}"
)

add_library(audio_studio_android SHARED
    native_bridge.cpp
    vad_jni.cpp
)
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
    ../../../../../../engine/src/transcription.cpp
    ../../../../../../engine/src/transcription_json.cpp
    ../../../../../../engine/src/audio_resampler.cpp
    ../../../../../../engine/src/wav_audio_reader.cpp
    ../../../../../../engine/src/vad.cpp
    ../../../../../../engine/src/silero_onnx.cpp
    ../../../../../../engine/src/native_vad_session.cpp
    ../../../../../../engine/src/native_vad_api.cpp
    ../../../../../../engine/src/speech_region_delivery.cpp
    ../../../../../../engine/src/capture_pcm_ring.cpp
    ../../../../../../engine/src/capture_vad_worker.cpp
    ../../../../../../engine/src/capture_vad_controller.cpp
    ../../../../../../engine/src/live_capture_vad.cpp
)
target_include_directories(audio_studio_core PUBLIC
    ../../../../../../engine/include
    "${ONNXRUNTIME_INCLUDE_DIR}"
)
target_compile_features(audio_studio_core PUBLIC cxx_std_17)
target_link_libraries(audio_studio_core PUBLIC
    dsp_engine_static
    noise_cancellation_static
    whisper
    onnxruntime
)

find_package(oboe REQUIRED CONFIG)

target_link_libraries(audio_studio_android
    PRIVATE audio_studio_core whisper onnxruntime oboe::oboe log dl m
)
