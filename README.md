# Audio Studio

**Working title:** Audio Studio  
**Status:** Native C++ foundation in progress  
**Targets:** Android and Windows  
**Product direction:** A local-first voice recording and narration production studio.

Audio Studio is intended to help a creator move from a script to clean, edited, publish-ready narration using their own voice—even when their recording environment is less than ideal.

The product roadmap covers dependable recording, speech cleanup, intelligent retakes, transcript-aware editing, script and teleprompter tools, optional AI narration, captions, and production handoff tools. The first release will focus on a reliable recording foundation rather than a large collection of unfinished features.

> “Audio Studio” is a working title. The repository name remains unchanged until a suitable product name is researched and selected.

## Technology decision

The app will **not use Flutter**. The audio workload is central enough that we are choosing a native architecture:

- **Shared C++ core:** audio types, DSP, project/audio processing logic, quality checks, and reusable algorithms.
- **Android:** Kotlin UI and lifecycle/permissions, Android NDK, and Google's Oboe for native low-latency audio capture/playback.
- **Windows:** C# with WinUI 3 for the desktop UI, plus a native C++ audio layer using Windows audio APIs (including WASAPI where appropriate).
- **Build system:** CMake for the shared C++ core; platform-native build tooling for Android and Windows.
- **Testing:** deterministic C++ unit tests and platform integration tests. Audio quality must also be checked with test recordings and listening tests.

This shares the code where sharing is valuable without pretending Android and Windows have identical audio-device, lifecycle, or permission behavior.

## Product principles

- **Voice first:** preserve natural speech; avoid metallic or over-processed sound.
- **Local first:** core recording, project storage, basic editing, and export should work offline wherever platform capabilities allow.
- **Non-destructive:** preserve original recordings; keep edits reversible.
- **Reliable before clever:** protect against dropouts, clipping, lost takes, and corrupt projects.
- **Native where it matters:** use platform audio APIs and lifecycle handling rather than routing real-time audio through UI code.
- **Optional cloud:** transcription, synthetic narration, and cloud assistance are integrations—not prerequisites for recording.
- **Evidence-led quality:** evaluate audio with repeatable tests, measurements, and listening—not marketing claims.

## Roadmap at a glance

1. **Native foundation:** C++ core, Android/Windows shells, microphone capture, safe file writing, and recovery.
2. **Recording controls:** input meters, monitoring where supported, device selection, take management, and baseline quality checks.
3. **Intelligent editor:** waveform editing, takes, transcript-aware editing, silence/filler review, and narration assembly.
4. **Creator studio:** scripts, teleprompter, timing, captions, production markers, templates, and batch export.
5. **Optional AI and advanced intelligence:** provider adapters, enhancement, session consistency checks, quality recommendations, and production blueprints.

See [the roadmap](docs/ROADMAP.md), [hardening backlog](docs/HARDENING_BACKLOG.md), [product requirements](docs/PRODUCT_REQUIREMENTS.md), and [architecture](docs/ARCHITECTURE.md).

## Repository structure

```text
engine/                 Shared C++ core and CMake build
platforms/android/      Android application (Kotlin + NDK/Oboe)
platforms/windows/      Windows application (C# + WinUI 3 + native bridge)
docs/                   Product, architecture, privacy, and testing documentation
tests/                  C++ unit tests and later platform/audio integration tests
.github/workflows/      Continuous integration
```

Platform folders and audio dependencies will be added in deliberate, buildable steps. Avoid adding a dependency before checking its maintenance, licensing, platform support, and measurable benefit.

## Development status

The repository contains a tested C++ core, an Android Kotlin/NDK shell linked to Oboe, and a Windows WinUI shell connected to a small C ABI bridge. CI builds the core on Linux and Windows and attempts to build the Android APK and Windows shell. These shells are not finished production suites; individual features must pass CI and platform validation before being described as complete.

## Processing status

The shared C++ core includes a validated PCM16 buffer reader, common PCM 8/16/24/32-bit and IEEE float32 input support for waveform analysis, trim, and streamed processing, a tested conservative noise gate, and a non-destructive WAV processing path. On Android, users can choose a PCM16 WAV and export a separate processed copy to Music/Audio Studio/Processed; the original is never overwritten. Processing streams audio in 4096-frame blocks rather than loading the entire file into memory, so there is no arbitrary 64 MiB file-size cap. Processing reports progress, supports cancellation between blocks, and removes incomplete output on cancellation or failure. Android also cleans up stale processing scratch files older than 24 hours. The gate attenuates quieter sections and is not advanced noise cancellation or echo removal. Audio quality still requires physical-device listening tests.

## Licensing

No project license has been selected yet. Third-party dependencies will be reviewed individually before adoption. In particular, we will not adopt a framework whose licensing obligations conflict with the intended distribution model.

Until a license is added, do not assume that the project source is available for reuse, redistribution, or commercial use.
