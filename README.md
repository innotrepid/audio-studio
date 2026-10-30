# Audio Studio

**Working title:** Audio Studio  
**Status:** Native Android and Windows application with an established shared C++ audio core; Creator Studio capabilities are planned in staged delivery  
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

1. **Record:** native capture, input monitoring, WAV finalization, and interrupted-recording recovery.
2. **Editor:** waveform analysis, precise non-destructive trim, offline processing, and original/processed review.
3. **Library:** discover recordings and derived WAV outputs using platform-appropriate file access.
4. **Creator Studio (planned):** projects, scripts, teleprompter, transcript/caption editing, and production markers.
5. **Advanced workflows (planned):** verified phrase replacement, non-destructive assembly, and optional AI integrations.

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

The platform folders contain the native applications and shared engine. Avoid adding a dependency before checking its maintenance, licensing, platform support, and measurable benefit.

## Development status

The repository contains an Android Kotlin/NDK application using Oboe and a Windows WinUI 3 application connected to the shared native C++ engine through a C ABI bridge. The shared engine includes WAV validation/decoding, waveform analysis, precise trim, and offline processing paths. Android and Windows include native recording workflows. CI validates the Android native shell/APK and Linux/Windows C++ core (and Windows shell where configured). These applications are still evolving; CI and real-device/manual validation are both required before describing a workflow as release-ready.

## Processing status

The shared C++ core supports common mono/stereo PCM 8/16/24/32-bit and IEEE float32 WAV input, plus standard WAVE_FORMAT_EXTENSIBLE PCM/float wrappers for waveform analysis, trim, and supported processing paths. Waveform analysis uses bounded reads and produces a min/max envelope; trim uses frame-accurate selections and exports a new WAV. Processing uses bounded blocks, reports progress, supports cancellation, and writes a separate output without replacing the source. Android's processing workflow includes RNNoise, DeepFilterNet3 (BETA), and a classic noise-gate option. RNNoise is the recommended lightweight neural option; DeepFilterNet3 is experimental and may use more CPU. Neural enhancement is offline processing, not a live filter in the recording callback. The classic gate attenuates quieter sections and is not spectral noise cancellation or dereverberation. Quality and performance still require physical-device listening tests.

## Licensing

No project license has been selected yet. Third-party dependencies will be reviewed individually before adoption. In particular, we will not adopt a framework whose licensing obligations conflict with the intended distribution model.

Until a license is added, do not assume that the project source is available for reuse, redistribution, or commercial use.

## Experience direction

The planned UI direction is **Hybrid Studio**: approachable Record and Creator workflows alongside precise Editor and Processing workspaces. The visual foundation uses near-black/charcoal surfaces, blue as the primary interactive accent, and mint for live/healthy audio feedback. Android and Windows share product concepts but retain native, platform-appropriate layouts. See [the redesign blueprint](docs/CREATOR_STUDIO_REDESIGN.md), [screen architecture](docs/SCREEN_ARCHITECTURE.md), and [UI roadmap](docs/UI-ROADMAP.md).
