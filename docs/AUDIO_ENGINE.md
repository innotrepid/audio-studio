# Audio Engine

## Goals

The engine must capture and save speech reliably before adding complex enhancement. It should preserve natural voice quality, avoid dropouts, and keep original recordings recoverable.

## Technology direction

- **Shared logic:** C++ with CMake, designed for deterministic tests.
- **Android audio I/O:** evaluate Google's Oboe through the Android NDK for low-latency native capture/playback; handle permissions and lifecycle in the Android layer.
- **Windows audio I/O:** use Windows audio APIs, including WASAPI where appropriate, with device changes and endpoint failures handled explicitly.
- **DSP:** implement and test independent processing units; do not run heavy enhancement in a real-time callback unless its timing and worst-case cost have been measured.
- **File writing:** use a bounded producer/consumer design so the capture callback does not wait on disk I/O.

## First implementation milestone

The first milestone is intentionally small:
1. Define explicit audio format and buffer types.
2. Validate core invariants with unit tests.
3. Build the shared C++ core on Linux and Windows in CI.
4. Add platform-native shells and microphone capture separately, with integration tests.
5. Record known test signals and voice samples, then verify duration, sample rate, channel count, clipping behavior, and file integrity.

The initial C++ core is not itself a recorder. Platform capture is only considered complete after it works on real devices.

## Audio callback constraints

- No blocking locks, disk writes, network requests, database access, or UI work in the callback.
- No unbounded allocation or unpredictable work in the callback.
- Communicate with worker threads using bounded queues and explicit overflow behavior.
- Detect clipping, dropped frames, stream errors, and interrupted sessions.
- Make sample rate, channel count, and sample representation explicit.

## Enhancement order

1. Safe recording and file finalization.
2. Playback and input monitoring with measured latency.
3. Basic gain, high-pass filtering, EQ, compression, and de-essing as non-destructive options.
4. Offline noise reduction and dereverberation evaluated on speech datasets and user listening tests.
5. Optional real-time enhancement only when CPU cost and artifacts are acceptable.

## Quality gates

Every audio feature needs automated tests for correctness and failure paths plus listening tests on representative voices, rooms, microphones, and background noise. Avoid claiming that ordinary filters remove severe room echo or noise; those problems may require more advanced algorithms or models.
