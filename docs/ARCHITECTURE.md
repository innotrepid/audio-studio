# Architecture

## Decision

Use a native, layered architecture rather than Flutter. Audio Studio targets Android and Windows, but those operating systems have different audio APIs, permission models, device lifecycles, and user-interface conventions.

- **Shared C++ core:** audio data types, DSP primitives, deterministic processing, file-format logic, and platform-independent project/audio rules. The offline WAV-effects adapter uses DSP-ENGINE at pinned commit `05c050c8ebfe3351612cde8cd7d7c94a7d3b1ec3`; it links the static core only and does not use DSP-ENGINE device backends.
- **Dependency note:** first CMake configure requires network access to fetch the pinned DSP-ENGINE revision. The exact commit pin prevents silent upstream changes; builds should be tested with the pin before updating it.
- **Android shell:** Kotlin for UI, permissions, lifecycle, and Android integration; C++ through the Android NDK; Oboe for low-latency audio I/O where suitable.
- **Windows shell:** C# and WinUI 3 for the desktop interface; native C++ for shared processing and Windows audio integration, using WASAPI where appropriate.
- **Boundary:** keep platform audio capture and lifecycle details behind small interfaces. Avoid passing audio sample buffers through UI events or high-level UI bindings.
- **Builds:** CMake for the shared C++ core; Gradle/Android NDK for Android; Visual Studio/MSBuild and Windows App SDK for Windows.

## Logical layers

1. **Presentation:** native Android and Windows screens, accessibility, controls, and view state.
2. **Application workflows:** recording session lifecycle, project management, take management, export jobs, and user-approved actions.
3. **Domain:** project, script, recording, take, edit decision, processing preset, export, and provider job.
4. **Shared C++ core:** audio formats/buffers, DSP, processing contracts, quality checks, and platform-independent algorithms.
5. **Platform audio adapters:** Oboe/Android lifecycle and Windows audio-device APIs; capture/playback behavior remains platform-specific.
6. **Persistence:** project metadata in SQLite or an equivalent local store; media files stored separately.
7. **Optional integrations:** transcription, synthetic narration, and cloud assistance behind explicit interfaces.

## Real-time audio rules

- Never perform blocking filesystem, database, network, UI, or logging operations inside a real-time audio callback.
- Use bounded buffers/queues and move encoding, disk writes, and heavier DSP to suitable worker threads.
- Define sample format, sample rate, channel count, frame count, and timestamps explicitly at subsystem boundaries.
- Handle start/stop races, device removal, app lifecycle changes, and failed writes.
- Measure end-to-end latency, CPU use, and dropouts on actual Android devices and Windows hardware.
- Do not let screen navigation accidentally destroy a recording session.
- Never claim noise reduction or dereverberation is production-ready without quality tests and listening evaluation.

## Project data and persistence

Keep project metadata separate from media bytes. Projects should reference immutable source recordings and derived renders; processing/edit decisions should be stored as metadata where feasible. Version every persisted project schema and provide explicit migrations before shipping project-format changes.

The Creator Studio implementation should begin with the smallest reliable local persistence design that supports atomic writes, autosave, recovery, and migrations. A versioned project document may be sufficient initially; evaluate SQLite only when query patterns, concurrent updates, or project scale justify it. Do not make a database choice a prerequisite for the visual foundation.

Never overwrite the only copy of an original take. Long-running work should be recoverable after an application interruption or unexpected shutdown.

## Current UI direction

Use a Hybrid Studio experience: approachable Record and Creator workflows, paired with precise Editor and Processing workspaces. Android and Windows share product concepts, terminology, and visual tokens but use platform-appropriate navigation and layouts. The primary interactive accent is blue; mint is reserved for live/healthy audio states.

## Proposed repository layout

```text
engine/
  include/audio_studio/    Public, platform-independent C++ interfaces
  src/                     Shared C++ implementation
  tests/                   Deterministic core tests
platforms/android/          Kotlin app, JNI bridge, NDK integration
platforms/windows/          WinUI app and native bridge
docs/                       Product and engineering decisions
```

## Licensing and dependencies

Review licensing, maintenance status, supported Android/Windows versions, architecture support, and offline distribution requirements before adding third-party code. A cross-platform audio framework such as JUCE is a possible alternative, but its AGPL/commercial licensing model must be evaluated before adoption. The current plan avoids committing to it before the product's distribution model is clear.
