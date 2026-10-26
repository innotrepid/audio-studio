# Changelog

## Unreleased

- Replaced the initial Flutter scaffold with a native C++ core architecture.
- Added CMake builds and deterministic audio-format/buffer validation tests.
- Added an Android Kotlin/NDK shell with Oboe linked through Gradle Prefab.
- Added a Windows C# / WinUI 3 shell with a narrow native C ABI bridge.
- Expanded CI to test the C++ core on Linux and Windows and build platform shell artifacts.
- Microphone capture and WAV recording are not implemented yet.
