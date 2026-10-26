# Android application

This is the initial Kotlin + Android NDK shell. It links the shared C++ audio types and the Oboe library through Prefab/CMake.

## Build

Open this folder in Android Studio with Android SDK 35, JDK 17, CMake 3.22.1, and Android NDK installed. The CI pipeline uses Gradle to build a debug APK.

## Current limitations

- This is a shell/integration milestone, not a recorder.
- Microphone permission and capture are not wired yet.
- Oboe is linked, but no audio stream is opened yet.
- Do not interpret the status label as proof that microphone hardware works.

The next milestone will implement safe microphone capture and WAV file finalization with explicit permission and failure handling.
