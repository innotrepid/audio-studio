# Synvidra

**Product name:** Synvidra  
**Status:** Native Android and Windows application with a shared C++ audio core; **Creator Studio is implemented through Phase 7.4** on the redesign branch (polish and release validation continue)  
**Targets:** Android and Windows  
**Product direction:** A local-first voice recording and narration production studio.

Synvidra helps a creator move from a script to clean, edited, publish-ready narration using their own voice—even when the recording environment is imperfect.

Synvidra is the product name. The repository name remains unchanged to preserve source and automation URLs.

## Technology decision

The app does **not use Flutter**. The audio workload is central enough that we use a native architecture:

- **Shared C++ core:** audio types, DSP, project/audio processing logic, quality checks, and reusable algorithms.
- **Android:** Kotlin UI and lifecycle/permissions, Android NDK, and Google's Oboe for native low-latency audio capture/playback.
- **Windows:** C# with WinUI 3 for the desktop UI, plus a native C++ audio layer using Windows audio APIs (including WASAPI where appropriate).
- **Build system:** CMake for the shared C++ core; platform-native build tooling for Android and Windows.
- **Testing:** deterministic C++ unit tests, Creator store unit tests, and platform integration tests. Audio quality must also be checked with test recordings and listening tests.

## Product principles

- **Voice first:** preserve natural speech; avoid metallic or over-processed sound.
- **Local first:** core recording, project storage, basic editing, and export should work offline wherever platform capabilities allow.
- **Non-destructive:** preserve original recordings; keep edits reversible. Phrase assembly writes **derived** WAVs only.
- **Reliable before clever:** protect against dropouts, clipping, lost takes, and corrupt projects.
- **Native where it matters:** use platform audio APIs and lifecycle handling rather than routing real-time audio through UI code.
- **Optional cloud:** transcription, synthetic narration, and cloud assistance are integrations—not prerequisites for recording.
- **Evidence-led quality:** evaluate audio with repeatable tests, measurements, and listening—not marketing claims.

## Capabilities (redesign branch tip)

### Record / Editor / Library

1. **Record:** native capture (Oboe / WASAPI path), input monitoring, WAV finalization, clipping feedback, interrupted-recording recovery.
2. **Editor:** waveform analysis, precise non-destructive trim, offline processing (RNNoise, DeepFilterNet3 BETA, classic gate), original vs derived review.
3. **Library:** discover recordings and derived WAV outputs using platform-appropriate file access.

### Creator Studio (implemented)

4. **Projects & scripts:** versioned project JSON, atomic save, recovery generations, section-linked takes, take approval states.
5. **Teleprompter:** Creator settings (speed, font, spacing, countdown), reading-position persistence, section navigation; Record-screen studio teleprompter bound to Creator data; record starts scroll, pause pauses both.
6. **Captions & production:** draft captions with timing provenance, transcript–take link, production markers.
7. **Phrase replacement:** manual range → link replacement take → A/B audition → assembly preview (soft ~15 ms crossfade) → approve (`derivedPath`) or discard; **source takes never overwritten**.
8. **Backup:** export/import validated project JSON (take paths only; no audio packaging). Invalid import leaves the active project unchanged.

See [Creator redesign blueprint](docs/CREATOR_STUDIO_REDESIGN.md), [Phase 7 readiness](docs/PHASE7_RELEASE_READINESS.md), and [roadmap](docs/ROADMAP.md).

## Repository structure

```text
engine/                 Shared C++ core and CMake build
platforms/android/      Android application (Kotlin + NDK/Oboe)
platforms/windows/      Windows application (C# + WinUI 3 + native bridge)
docs/                   Product, architecture, privacy, and testing documentation
tests/                  C++ unit tests and later platform/audio integration tests
scripts/                CI apply helpers (large-file safe UI/store patches)
.github/workflows/      Continuous integration
```

## Development status

CI validates the Android native shell/APK and Linux/Windows C++ core (and Windows shell where configured), including Creator persistence tests. CI success is not proof of visual usability or listening quality; physical-device and desktop manual checks remain required before release claims.

## Processing status

The shared C++ core supports common mono/stereo PCM and IEEE float32 WAV input for waveform analysis, trim, and supported processing paths. Processing uses bounded blocks, reports progress, supports cancellation, and writes a separate output without replacing the source. Neural enhancement is offline processing, not a live filter in the recording callback.

## Licensing

No project license has been selected yet. Third-party dependencies are reviewed individually before adoption.

Until a license is added, do not assume that the project source is available for reuse, redistribution, or commercial use.

## Experience direction

**Hybrid Studio:** approachable Record and Creator workflows alongside precise Editor and Processing workspaces. Near-black/charcoal surfaces, blue primary accent, mint for live/healthy audio feedback. Android and Windows share product concepts but retain native layouts.
