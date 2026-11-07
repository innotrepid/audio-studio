# Changelog

## Unreleased

### Current repository state

- Native Android application using Kotlin, Android NDK, and Oboe; native Windows application using C# / WinUI 3.
- Shared C++ audio core with CMake and deterministic regression tests.
- Native microphone recording workflows, WAV finalization, recording status, input-level feedback, and interrupted-recording recovery.
- WAV waveform analysis, frame-accurate non-destructive trim, selected-range preview, and separate derived exports.
- Offline processing includes RNNoise, experimental DeepFilterNet3 (BETA), and a classic noise-gate path where supported by the platform UI.
- Processing supports progress/cancellation and preserves the source recording.
- CI covers the shared C++ core on Linux and Windows and the Android/Windows native build paths configured in the repository, including Creator store unit tests.

### Creator Studio (Phases 3–7.4 on redesign tip)

- Creator destination with Scripts, Teleprompter, Captions, and Production workspaces.
- Versioned project/script persistence with atomic save and recovery generations.
- Teleprompter preferences, reading-position persistence, section navigation; Record studio teleprompter driven by Creator data; coupled record/scroll transport.
- Caption timing provenance, take approval states, transcript–take link, production markers.
- Phrase replacement pipeline: manual range, replacement take link, A/B audition, assembly preview with ~15 ms linear crossfade, approve (derived WAV + `derivedPath`) or discard; source takes never overwritten.
- Project JSON export/import with validation; failed import does not replace the active project; take audio is not packaged.
- Accessibility: Android content descriptions and Windows AutomationProperties.Name on key Creator / teleprompter controls.

### Design and product work

- Hybrid Studio direction: approachable Creator workflows paired with precise audio editing and processing tools.
- Navigation: Record, Editor, Creator, and Library; Settings remains secondary.
- Remaining Phase 7 work: broader accessibility/keyboard pass, manual device/desktop smoke evidence, and final release packaging checks. See `docs/PHASE7_RELEASE_READINESS.md`.

### Known validation limits

- CI success proves only the checks executed by CI passed.
- Physical Android device checks and Windows runtime checks are still required for platform behavior.
- Neural enhancement quality, DeepFilterNet3 performance, and phrase-replacement assembly still benefit from dedicated listening fixtures before production quality claims.
