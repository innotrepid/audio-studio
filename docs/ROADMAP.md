# Roadmap

The roadmap is ordered by technical risk. The shared engine and real-device recording path come before editing features or AI.

## Phase 0 — Native foundation (current)

- [x] Select a native architecture for the audio-focused product.
- [ ] Add a small, testable C++ core using CMake.
- [ ] Compile and test the core on Linux and Windows in CI.
- [ ] Add Android and Windows native application shells.
- [ ] Record licensing and dependency decisions.

**Exit criteria:** green C++ CI, documented architecture, and no claims that recording or enhancement works before platform validation.

## Phase 1 — Reliable recording

- Implement Android microphone capture with Oboe/NDK and Windows capture using native Windows audio APIs.
- Add explicit permission/device handling and lifecycle recovery.
- Write audio to a safe temporary file and finalize atomically.
- Support WAV/PCM first; add other codecs only after reviewing compatibility and licensing.
- Add recording state, duration, input meters, clipping warnings, and recoverable takes.
- Test device changes, interruption, low storage, failed writes, and abrupt app termination.

**Exit criteria:** repeatable recordings on real Android hardware and Windows, no known data-loss path in the tested scenarios, and documented limitations.

## Phase 2 — Monitoring and basic processing

- [x] Add a conservative, tested speech noise-gate/expander foundation to the shared C++ core; keep it opt-in and out of the raw capture path.
- Playback, monitoring where supported, and device/latency diagnostics.
- Non-destructive gain, high-pass filter, EQ, compression, and de-essing.
- Audio test fixtures and listening evaluations.

## Phase 3 — Editing and creator tools

- Waveform and take editing.
- Script editor and teleprompter.
- Transcript-aware editing, silence/filler review, captions, and export.
- Project templates and production markers.

## Phase 4 — Enhancement and optional AI

- Evaluate local noise reduction and dereverberation for quality, performance, model size, and license.
- Optional provider integrations for transcription or synthetic narration.
- Preserve offline recording/editing if providers are unavailable.

## Phase 5 — Advanced intelligence

- Session consistency checks, quality recommendations, production blueprints, and batch workflows.

## Definition of done

A feature is complete only when it builds on its target platforms, has tests for normal and failure paths, preserves original recordings, and has clear user-facing limitations. A successful compile alone is not evidence of audio quality.


## Noise-reduction foundation follow-through
- [x] Parse and validate mono/stereo PCM16 WAV files.
- [x] Add non-destructive noise-gate processing and a separate WAV output.
- [x] Add Android document-picker and processed-copy export path.
- [ ] Validate processed output on a physical Android device with quiet speech gaps and real background noise.
- [ ] Replace whole-file decoding with bounded streaming before raising or removing the 64 MiB input cap.
- [ ] Add A/B playback for original-versus-processed comparison.
- [x] Add persistent Android user controls for gate threshold and attenuation after initial device listening checks.
- [ ] Evaluate spectral noise reduction and dereverberation separately; do not describe the gate as AI cancellation.
