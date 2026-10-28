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

## Delivery cadence: avoid endless hardening

Reliability work is tracked in [HARDENING_BACKLOG.md](HARDENING_BACKLOG.md). Fix confirmed data-loss, corruption, security, and crash risks promptly. Park lower-priority concerns with a revisit trigger. After the current in-flight hardening item is merged, resume product implementation; do not start another broad hardening sweep without a trigger.

**Current product milestone:** verified non-destructive trim export is implemented on Android and Windows. Both platforms support precise in/out timecodes (`MM:SS.mmm`) mapped to audio-frame positions for preview and export; slider adjustments remain available and intentionally replace precision for the moved endpoint. Windows supports selected-range preview, and Android preserves the exact selected frames through native export. Direct waveform trim handles are implemented on both platforms. The current Windows preview-safety change cancels playback when the selection or source changes, including while the media pipeline is still opening. The original file remains unchanged. Continue editing workflow capability work without broadening into a visual redesign.

## Definition of done

A feature is complete only when it builds on its target platforms, has tests for normal and failure paths, preserves original recordings, and has clear user-facing limitations. A successful compile alone is not evidence of audio quality.


## Noise-reduction foundation follow-through
- [x] Parse and validate mono/stereo PCM16 WAV files.
- [x] Add non-destructive noise-gate processing and a separate WAV output.
- [x] Add Android document-picker and processed-copy export path.
- [ ] Validate processed output on a physical Android device with quiet speech gaps and real background noise.
- [x] Replace whole-file decoding with bounded streaming; process fixed-size PCM16 blocks while preserving noise-gate state.
- [x] Add background processing progress and safe cancellation between blocks, including cleanup of incomplete output.
- [x] Add Android A/B playback for the latest original-versus-processed WAV pair.
- [x] Add persistent Android user controls for gate threshold and attenuation after initial device listening checks.
- [ ] Evaluate spectral noise reduction and dereverberation separately; do not describe the gate as AI cancellation.
