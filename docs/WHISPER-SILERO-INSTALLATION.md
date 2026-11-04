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

### Capture wiring
**Android:** Oboe dual-submit into `LiveCaptureVad`; Kotlin starts with bundled `silero_vad.onnx` via `BundledModelStore`, polls regions on the main thread.
**Windows:** WASAPI dual-submit into `LiveCaptureVad` via `audio_studio_recording_start_with_vad`; soft-fail if the model is missing. UI uses `OfflineSpeechNative.TryStartRecordingWithBundledVad`.

### Whisper UI entry points
- **Windows:** `audio_studio_transcribe_wav` + Creator Captions button "Transcribe last recording (offline)" (`OfflineSpeechNative.TranscribeWavAsync`).
- **Android:** existing JNI `nativeTranscribe` + bundled model path via `BundledModelStore` / `ensureTranscriptionModel`.

## Still required before product claims

### Validation
1. Fresh install offline: open Silero session and run one push/flush cycle on device.
2. Recorded-speech fixture coverage before accuracy claims in UI/changelog.
3. Keep package size and memory budgets explicit when changing the default Whisper variant.
4. Windows device smoke: record with VAD status, stop, drain regions; transcribe a short WAV offline.

## Local developer setup

```bash
python scripts/fetch_transcription_models.py --output build/transcription-models
```

Android Gradle extracts ONNX Runtime into `app/build/onnxruntime-native` during `preBuild` and fails clearly if offline models are missing.
