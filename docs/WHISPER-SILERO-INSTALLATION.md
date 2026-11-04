# Whisper and Silero installation status

Goal: after installing Synvidra on Android or Windows, Whisper (speech-to-text) and Silero VAD work **offline** with no post-install model download.

## Done

### Packaging
- Pinned artifacts in `models/transcription-models.json` (`ggml-tiny.bin`, `silero_vad.onnx`) with exact size + SHA-256.
- `scripts/fetch_transcription_models.py` downloads and verifies those artifacts.
- **Windows CI / publish:** copies both models into `models/` next to the app; verifies checksums.
- **Android CI / APK:** fetches models, copies into `assets/models/`, verifies the APK embeds both files.
- Platform helpers: Windows `BundledModelPaths`, Android `BundledModelStore`.

### Silero engine availability
- **Windows:** production `audio_studio_vad_create` / UTF-16 paths + ONNX Runtime 1.23.2 (existing).
- **Android:** production `audio_studio_vad_create_utf8`, NDK-linked `silero_onnx.cpp`, extracted `onnxruntime-android:1.23.2` headers/`.so`, Kotlin `SileroVadSession` opening the bundled model offline.
- Capture worker/controller sources are linked into the Android native library for the next wiring step.

## Still required before product claims

### Capture wiring
**Android (merged):** Oboe dual-submit into `LiveCaptureVad`; Kotlin starts with bundled `silero_vad.onnx` via `BundledModelStore`, polls regions on the main thread.
1. ~~Own a per-recording VAD lifecycle~~ — `nativeStartRecordingWithVad` + soft-fail backpressure.
2. ~~Capture callback only writes to ring~~ — RT `submit_audio` only.
3. Pause/resume/stop/flush path finishes the worker; region delivery is bounded.
4. Kotlin drains `nativePopVadRegion` without blocking Oboe.

**Windows (still open):** WASAPI capture dual-submit into the same engine path.

### Whisper UI entry points
1. Decode → resample 16 kHz mono → `transcribe()` on a background thread.
2. Resolve model via `BundledModelPaths` / `BundledModelStore`.
3. Offline CI/device smoke with network disabled.

### Validation
1. Fresh install offline: open `SileroVadSession` and run one push/flush cycle.
2. Recorded-speech fixture coverage before accuracy claims in UI/changelog.
3. Keep package size and memory budgets explicit when changing the default Whisper variant.

## Local developer setup

```bash
python scripts/fetch_transcription_models.py --output build/transcription-models
```

Android Gradle extracts ONNX Runtime into `app/build/onnxruntime-native` during `preBuild` and fails clearly if offline models are missing.
