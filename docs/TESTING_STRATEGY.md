# Testing Strategy

## Test layers

### Static checks
- Dart formatting.
- Flutter/Dart static analysis.
- Dependency and license review.
- Platform build checks where runners support them.

### Unit tests
- Project and take models.
- Recording-session state transitions.
- Error mapping and recovery logic.
- Processing-preset validation.
- Export naming and metadata.
- Project schema migrations.

### Integration tests
- Permission granted/denied.
- Start, stop, and finalize recording.
- Playback of a completed file.
- Device unavailable or removed.
- Interrupted recording and recovery.
- Low storage/write failure behavior.
- Project reopen and export verification.

### Audio tests
- Known test signals for sample format, channels, duration, and clipping behavior.
- Repeatable speech fixtures for noise-reduction comparisons.
- Regression checks for unexpected silence, truncation, distortion, or channel swapping.
- Manual listening evaluation for naturalness and intelligibility.

### Performance tests
- Capture dropouts and buffer underruns.
- CPU and memory under sustained recording.
- Startup time and stop/finalization latency.
- Processing duration versus source duration.
- Battery impact on Android where applicable.

## Manual hardware matrix

For each release candidate, record:
- Device and OS version.
- Microphone/input route.
- Sample rate and channel configuration.
- Duration of test recording.
- Whether the app was backgrounded or interrupted.
- Result and relevant logs (without private audio content).

At minimum, test a real Android device and a real Windows computer before claiming both are ready.

## Release gates

- CI checks pass.
- No known data-loss or corrupted-output defect in the tested recording flow.
- Permission, interruption, and error paths have been exercised.
- Audio artifacts and limitations are documented.
- Original recording remains intact after processing.
- Versioning and migration behavior are tested when project formats change.

A green CI run proves only the checks executed by CI passed. It does not establish microphone quality, noise-removal quality, or real-device reliability.
