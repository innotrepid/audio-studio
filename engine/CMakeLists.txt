add_library(audio_studio_core
    src/audio_types.cpp
    src/wav_writer.cpp
    src/noise_gate.cpp
)

target_include_directories(audio_studio_core PUBLIC include)
target_compile_features(audio_studio_core PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(audio_studio_core PRIVATE /W4 /WX)
else()
    target_compile_options(audio_studio_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(BUILD_TESTING)
    add_executable(audio_studio_core_tests tests/audio_types_test.cpp)
    target_link_libraries(audio_studio_core_tests PRIVATE audio_studio_core)
    target_compile_features(audio_studio_core_tests PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(audio_studio_core_tests PRIVATE /W4 /WX)
    else()
        target_compile_options(audio_studio_core_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME audio_studio_core_tests COMMAND audio_studio_core_tests)
endif()

    add_executable(audio_studio_wav_writer_tests tests/wav_writer_test.cpp)
    target_link_libraries(audio_studio_wav_writer_tests PRIVATE audio_studio_core)
    target_compile_features(audio_studio_wav_writer_tests PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(audio_studio_wav_writer_tests PRIVATE /W4 /WX)
    else()
        target_compile_options(audio_studio_wav_writer_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME audio_studio_wav_writer_tests COMMAND audio_studio_wav_writer_tests)

    add_executable(audio_studio_noise_gate_tests tests/noise_gate_test.cpp)
    target_link_libraries(audio_studio_noise_gate_tests PRIVATE audio_studio_core)
    target_compile_features(audio_studio_noise_gate_tests PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(audio_studio_noise_gate_tests PRIVATE /W4 /WX)
    else()
        target_compile_options(audio_studio_noise_gate_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME audio_studio_noise_gate_tests COMMAND audio_studio_noise_gate_tests)
