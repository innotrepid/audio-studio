# Silero VAD ONNX Runtime integration

## Runtime contract

- Silero model revision: `60b7ffa243625ebdc1070275a29f18c87843786a`
- Model file: `silero_vad.onnx`, MIT, SHA-256 `1a153a22f4509e292a94e67d6f9b85e8deb25b4988682b7e174c65279d8788e3`
- Input audio: mono float32 PCM, 16,000 Hz, exact **512 new samples** per hop (32 ms).
- Model input tensor: `input` float32 **[1, 576]** = 64 samples of prior context + 512 new samples. Context is not counted as newly analyzed time.
- Other inputs: `state` float32 [2, 1, 128], `sr` int64 scalar (16000).
- Outputs: `output` float32 [1, 1] probability, `stateN` float32 [2, 1, 128].
- Initialize recurrent state to zeros at each independent stream; feed `stateN` back as `state` for each subsequent window. Reset on stream end, sample-rate change, or explicit reset.
- Speech region timestamps are milliseconds on the **16 kHz mono timeline** after capture resampling. They align with source duration but are not original device frame indices.

## Dependency strategy

Pin ONNX Runtime to **1.23.2** for the first integration pass:
- Android: use the official Maven Central `com.microsoft.onnxruntime:onnxruntime-android:1.23.2` AAR, extract its headers and ABI-matched `libonnxruntime.so` for native C++ CMake linking, and package the matching shared library in the APK.
- Windows x64: use the official `Microsoft.ML.OnnxRuntime` NuGet package version `1.23.2` as the source of the C/C++ headers, import library, and `onnxruntime.dll`; copy the DLL beside the self-contained Synvidra executable.
- Keep dependency version pinned and make CI fail if the expected headers/native library are absent. Preserve the ONNX Runtime MIT license and notices in distribution artifacts.
- Keep inference CPU-only initially. Do not introduce GPU execution providers or runtime downloads.

## Shared API

The shared engine should expose a result-based operation taking a model path, mono 16 kHz samples, and VAD options, returning validated speech regions plus a structured error. ONNX session ownership and recurrent state belong to a per-stream runtime object, not global mutable state. The existing `build_speech_regions` function remains pure postprocessing and stays independently testable without loading ONNX Runtime.

### Platform availability (current)

- **Windows:** production `audio_studio_vad_create` loads the pinned Silero ONNX model and is covered by the Windows C++ core CI with real inference.
- **Android / Linux:** shared coordinator, resampler, and pure region postprocessing are available; production ONNX Runtime packaging for the public C VAD create path is still gated. Do not claim live Silero VAD on those targets until the packaging gates below are green.

## Validation gates

1. Compile ONNX Runtime adapter on Linux, Windows, and Android supported ABIs.
2. Smoke-test session creation and tensor names/shapes against the pinned model (input rank/shape must match **[1, 576]**).
3. Test zero-input inference, finite probability in [0,1], state propagation, reset isolation, and invalid request handling.
4. Confirm APK contains ABI-matched ONNX Runtime libraries and the verified Silero model.
5. Confirm Windows self-contained artifact contains `onnxruntime.dll` and the verified Silero model.
6. Keep tests bounded and deterministic; never run model inference as part of the pure validation test.
7. Verify offline operation after installation, without model/dependency downloads at app runtime.

## References

- ONNX Runtime Android C/C++ packaging: https://onnxruntime.ai/docs/install/
- ONNX Runtime 1.23.2 Android artifact: https://central.sonatype.com/artifact/com.microsoft.onnxruntime/onnxruntime-android/1.23.2
- ONNX Runtime 1.23.2 Windows native NuGet package: https://www.nuget.org/packages/Microsoft.ML.OnnxRuntime/1.23.2
- Silero upstream ONNX wrapper and recurrent-state handling: https://github.com/snakers4/silero-vad/blob/60b7ffa243625ebdc1070275a29f18c87843786a/src/silero_vad/utils_vad.py


## Current validation status

The Windows ONNX-backed test target now exercises the pinned model with real inference:

- Loads the manifest-pinned model and validates adapter initialization.
- Rejects null and non-finite inference windows without overwriting the caller's probability output.
- Runs 20 exact 512-sample windows of digital silence and requires each probability to remain at or below 0.1.
- Resets recurrent state, runs 60 windows of a deterministic voiced harmonic signal, and requires peak probability to reach at least 0.5.
- Resets state again and exercises the public C API push, flush, repeated-flush, and reset lifecycle.

The test is part of the Windows C++ core CI. Latest verified run for commit `b6dc19e12552d30743c29d0ee2cde73eb89b8efc` passed Windows C++ core, Linux C++ core, and Android native shell/APK jobs.

### Still required before claiming recorded-audio accuracy

- Add a small, license-compatible recorded speech fixture with a pinned source and checksum.
- Define expected speech intervals and acceptable timestamp tolerances from a reproducible reference.
- Run the streaming VAD over the fixture and assert detection coverage, false-positive behavior on nonspeech spans, and boundary tolerance.
- Expand ONNX-backed execution beyond Windows when the production ONNX Runtime targets are enabled on Linux/Android.

Synthetic inference validates that the pinned model executes and responds plausibly; it is not a substitute for recorded-speech accuracy benchmarking.
