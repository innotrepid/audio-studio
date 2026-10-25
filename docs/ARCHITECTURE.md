# Architecture

## Direction

Use Flutter/Dart for the shared application UI and project workflows, with a dedicated audio subsystem for latency-sensitive capture and processing. Platform-specific native code may be required for reliable device access and low-latency audio. The exact bridge and libraries are decisions to validate, not assumptions already proven.

## Logical layers

1. **Presentation:** screens, controls, accessibility, and view state.
2. **Application workflows:** recording session lifecycle, projects, take management, export jobs, and user-approved actions.
3. **Domain models:** project, script, recording, take, edit decision, processing preset, export, and provider job.
4. **Audio service boundary:** start/stop capture, input meters, playback, device/interruption events, file finalization, and processing jobs.
5. **Platform adapters:** Android and Windows capture/device APIs, permissions, audio focus, and filesystem behavior.
6. **Persistence:** project metadata and settings in SQLite or an equivalent local store; audio files stored separately.
7. **Optional integrations:** transcription, synthetic narration, and cloud assistance behind explicit interfaces.

## Audio threading rules

- Audio callbacks must not wait for Flutter UI, database, network, logging, or filesystem operations.
- Use bounded buffers/queues and move encoding, disk writes, and heavier DSP to suitable worker threads.
- Define sample format, sample rate, channels, frame count, and timestamps explicitly at subsystem boundaries.
- Handle start/stop races, device removal, app lifecycle changes, and failed writes.
- Measure end-to-end latency and dropouts on real hardware.
- The UI must not own the lifetime of an audio stream in a way that causes accidental termination during navigation.

## Project data

Keep metadata separate from media bytes. A project should reference original recordings and derived renders, with processing/edit decisions stored as metadata where feasible. Each project and export should have a schema/version marker so future releases can migrate safely.

Avoid overwriting the only copy of an original take. Long-running work should be recoverable after an application interruption.

## Proposed repository layout

```text
lib/                         Flutter entry point and UI
packages/audio_core/         Shared audio types and contracts (when justified)
packages/recording_engine/   Capture lifecycle and platform bridge (when justified)
packages/audio_processing/   DSP and enhancement adapters (when justified)
packages/project_storage/    Project metadata and media references (when justified)
packages/export_engine/      Export orchestration and format handling (when justified)
integration/                 Optional external provider adapters
test/                        Unit, integration, audio, and performance tests
docs/                        Product and engineering documentation
```

Do not split code into separate packages until the dependency boundary is clear. Early modularity should make platform behavior testable, not add ceremony.

## Decisions still open

- Native capture API/library for each platform.
- Audio file container/codec strategy and any MP3 encoder licensing implications.
- Real-time versus offline enhancement boundaries.
- Local speech recognition model and distribution size.
- Whether native audio components use C++, Rust, or a suitable maintained library.
- Project bundle/export format and migration policy.

Document each choice with alternatives, licensing, performance measurements, and platform limitations.
