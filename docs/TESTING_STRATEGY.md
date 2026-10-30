# Testing Strategy

## Test layers

### Static and build checks

- CMake configure/build and C++ compiler diagnostics.
- C++ unit/regression tests on Linux and Windows.
- Android Gradle/Kotlin/NDK build and native integration checks configured in CI.
- Windows WinUI/.NET/native bridge build checks configured in CI.
- Dependency and license review when dependencies change.
- Do not list Flutter or Dart checks: the product uses native Kotlin/Android and C++ plus C#/WinUI on Windows.

### C++ unit and regression tests

- Audio format parsing, validation, and buffer invariants.
- WAV chunk traversal, malformed/truncated input, and supported PCM/float formats.
- Bounded decoding and waveform envelope generation.
- Frame-accurate trim ranges and PCM16 export behavior.
- Processing parameter validation, block continuity, progress, cancellation, and cleanup.
- Source preservation and no-overwrite publication behavior.

### Platform and application tests

- Recording-session state transitions and native capture lifecycle.
- Permission granted/denied and retry.
- Start, stop, WAV finalization, playback, and repeated sessions.
- Input device unavailable/removed and route changes.
- Interrupted recording and recovery.
- Low storage, failed writes, and finalization errors.
- Processing from a selected file, cancellation, output validation, and source preservation.
- UI navigation, selection/state retention, scrolling, and incremental updates.

### Creator tests (as features are implemented)

- Project schema validation, migration, atomic save, autosave, and recovery.
- Script/section create, edit, reorder, and reload.
- Teleprompter pace/font/spacing persistence, automatic scroll, manual scroll, pause/resume, and section navigation.
- Transcript segment and caption cue timing validation.
- SRT/WebVTT export ordering, timestamps, encoding, and round-trip behavior.
- Production marker persistence and references.
- Phrase replacement requires verified audio-frame/time anchors and must preserve original and replacement source takes.

## Recording hardening checklist

Before treating Android capture as hardened on a device, verify:

- Record and save three consecutive sessions; each file must open and play independently.
- Record for at least five minutes; compare WAV duration and file size with the captured session.
- Background/interruption behavior is clear and leaves a valid finalized or recoverable file.
- Deny microphone permission, then grant it and retry; no stale recording state remains.
- Simulate storage/export failure where practical; never report success when writing, finalization, or WAV validation fails.
- Confirm elapsed time advances while recording and resets on the next session.

These checks require a physical Android device; CI does not validate hardware microphone behavior.

## Audio quality and performance tests

- Known test signals for sample format, channels, duration, and clipping.
- Repeatable speech fixtures for noise-reduction comparisons.
- Regression checks for unexpected silence, truncation, distortion, or channel swapping.
- Structured listening evaluation for naturalness and intelligibility.
- Capture dropouts, buffer underruns, CPU/memory under sustained recording, startup/stop latency, processing duration, and Android battery impact where applicable.

## Manual hardware matrix

For each release candidate, record:

- Device and OS version.
- Microphone/input route.
- Sample rate and channel configuration.
- Recording duration and interruption conditions.
- Processing engine/preset and relevant settings.
- Result and relevant logs, without private audio content.

At minimum, test a real Android device and a real Windows computer before claiming both platforms are ready.

## UI redesign validation

- Repeated navigation does not reset loaded files, waveform peaks, trim selection, or processing preferences.
- Slider/trim gestures update only affected controls and do not reload/re-analyze audio.
- Android scrolling and font scaling remain usable on small screens.
- Windows keyboard navigation and visible focus work.
- Empty, loading, permission-denied, no-match, and failure states remain distinct and actionable.

## Release gates

- All configured CI jobs pass.
- No known data-loss or corrupted-output defect in the tested workflows.
- Permission, interruption, cancellation, and error paths have been exercised.
- Audio artifacts and limitations are documented.
- Original recordings remain intact after processing, trim, and assembly.
- Project versioning and migration behavior are tested when project formats change.

A green CI run proves only the checks executed by CI passed. It does not establish microphone quality, noise-removal quality, visual usability, or real-device reliability.
