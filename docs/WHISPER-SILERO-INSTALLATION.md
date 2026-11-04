# Whisper and Silero installation status

Goal: after installing Synvidra on Android or Windows, Whisper (speech-to-text) and Silero VAD work **offline** with no post-install model download.

## Done in this packaging pass

- Pinned artifacts remain in `models/transcription-models.json` (`ggml-tiny.bin`, `silero_vad.onnx`) with exact size + SHA-256.
- `scripts/fetch_transcription_models.py` downloads and verifies those artifacts.
- **Windows CI / publish:** copies both models into `models/` next to the app output and self-contained publish directory; verifies checksums.
- **Android CI / APK:** fetches models before Gradle, copies them into `assets/models/`, builds the APK, and verifies the APK embeds both files with matching size/hash.
- Platform helpers resolve packaged paths:
  - Windows: `BundledModelPaths` under the executable `models/` folder.
  - Android: `BundledModelStore` extracts assets to app-private storage once, then returns absolute filesystem paths for native APIs.

## Still required before product claims

These are intentionally **out of scope** for the packaging PR and must not be described as finished until implemented and tested.

### Android Silero production inference

1. Link `silero_onnx.cpp` against ABI-matched ONNX Runtime **native** headers/`libonnxruntime.so` in the NDK CMake target (the Maven AAR alone is not sufficient for the C++ adapter).
2. Un-gate `audio_studio_vad_create` for non-Windows (UTF-8 model paths).
3. Device/CI smoke: load `silero_vad.onnx` offline and run one inference window.

### Capture and UI wiring (both platforms)

1. Own a per-recording VAD lifecycle in the capture worker (model path + options before start).
2. Feed the same PCM to WAV recording and VAD without blocking the real-time callback.
3. Pause/resume/stop/flush/error/reset behavior with bounded region delivery.
4. UI entry points that call `transcribe()` with decoded/resampled 16 kHz mono PCM on a background thread.

### Validation

1. Fresh install with network disabled: resolve both model paths and run one Whisper pass + one Silero session.
2. Recorded-speech fixture coverage before accuracy claims in UI/changelog.
3. Keep package size and memory budgets explicit when changing the default Whisper variant.

## Local developer setup

```bash
python scripts/fetch_transcription_models.py --output build/transcription-models
```

Windows builds expect that directory (or CI-equivalent) before packaging. Android Gradle copies from `build/transcription-models` into app assets during `preBuild`; if the directory is missing, the copy task fails with a clear error.
