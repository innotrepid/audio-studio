# Live capture VAD

## Engine

`audio_studio::LiveCaptureVad` owns:

1. **RT-safe** `submit_audio()` into `CapturePcmRing` (no ONNX on the audio callback).
2. A worker thread that runs `CaptureVadController` → `CaptureVadWorker` → Silero.
3. `pop_region()` for UI consumption (16 kHz mono timeline, milliseconds).

Create with the packaged `silero_vad.onnx` path and the capture stream’s sample rate/channel count.

Sources:

- `engine/include/audio_studio/live_capture_vad.hpp`
- `engine/src/live_capture_vad.cpp`

Linked from desktop `audio_studio_core`, Android NDK core, and Windows `audio_studio_native`.

## Platform wiring status

| Platform | Status |
|----------|--------|
| Engine pipeline | Done |
| Android Oboe dual-submit | Done — optional model path on start, submit in capture callback, finish on stop; Kotlin region poll |
| Windows WASAPI | Done — `audio_studio_recording_start_with_vad`, dual-submit after float convert, finish on stop |
| UI region list | Android status line poll; Windows can drain via `audio_studio_recording_pop_vad_region` / `OfflineSpeechNative` |

Recording must remain functional if VAD is disabled or soft-fails under backpressure.

## Validation

1. Unit-level: ring/worker/controller tests (existing); Windows live-capture integration test with ORT.
2. Device: start recording with bundled Silero path, speak, stop, drain regions offline.
3. Confirm WAV finalize is unchanged when VAD is on or off.
