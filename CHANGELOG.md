# Changelog

## Unreleased

### Current repository state

- Native Android application using Kotlin, Android NDK, and Oboe; native Windows application using C# / WinUI 3.
- Shared C++ audio core with CMake and deterministic regression tests.
- Native microphone recording workflows, WAV finalization, recording status, input-level feedback, and interrupted-recording recovery are implemented in the platform applications.
- WAV waveform analysis, frame-accurate non-destructive trim, selected-range preview, and separate derived exports are implemented.
- Offline processing includes RNNoise, experimental DeepFilterNet3 (BETA), and a classic noise-gate path where supported by the platform UI.
- Processing supports progress/cancellation and preserves the source recording.
- CI covers the shared C++ core on Linux and Windows and the Android/Windows native build paths configured in the repository.

### Design and product work

- Hybrid Studio direction: approachable Creator workflows paired with precise audio editing and processing tools.
- Updated navigation concept: Record, Editor, Creator, and Library; Settings remains secondary.
- Creator Studio capabilities—projects, scripts, teleprompter, transcript/caption editing, production markers, and phrase replacement—are planned in staged implementation and must not be treated as shipped until implemented and tested.

### Known validation limits

- CI success proves only the checks executed by CI passed.
- Physical Android device checks and Windows runtime checks are still required for platform behavior.
- Neural enhancement quality, DeepFilterNet3 performance, and phrase-replacement assembly require dedicated listening and regression validation before production claims.
