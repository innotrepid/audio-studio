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

## WAV reader foundation

The shared C++ core now has a validated PCM16 WAV reader for mono and stereo files. It walks RIFF chunks rather than assuming that the format and data chunks are at fixed offsets, skips unknown chunks and odd-byte padding, checks declared sizes and frame alignment, and leaves the caller's existing audio buffer untouched if loading fails. Decoded samples are normalized to float for DSP.

Waveform analysis, trim, and the streaming noise-gate export path accept mono/stereo integer PCM WAV at 8, 16, 24, or 32 bits, plus IEEE float32 WAV. Input samples are normalized to float; derived trim/processed outputs are written as PCM16 WAV. RIFF chunks are parsed with bounds checks and audio is processed in fixed 4,096-frame blocks. Compressed WAV and multi-data-chunk files remain unsupported and are rejected rather than guessed. Standard WAVE_FORMAT_EXTENSIBLE wrappers are accepted when they identify PCM or IEEE float, the valid-bit count matches the container bit depth, and the channel layout is mono/stereo. The standalone WavReader API still decodes PCM16 into a whole AudioBuffer; broader resource-bounded reader work remains deferred.

## Noise-reduction foundation

The shared core includes an optional, non-destructive `NoiseGate` processor. It is a conservative downward expander intended to attenuate low-level room/electronic noise between phrases while leaving louder speech close to unity gain. Its threshold, attenuation floor, attack, and release are configurable and validated.

Important limits:
- This is **not** AI noise cancellation, spectral denoising, or dereverberation. It will not reliably remove noise that overlaps speech, traffic, fans at speech level, or room echo.
- The processor is a tested engine component, not enabled in the capture-to-WAV path and not exposed in the Android UI. Raw recordings remain unchanged.
- It operates on normalized float buffers and maintains state between consecutive blocks; callers should use a separate processed copy to preserve the original take.
- Listen for pumping, clipped word beginnings/endings, and changes to quiet speech before using it in a production workflow.

The next enhancement step is a controlled preview/apply workflow that writes a separate processed WAV, compares it against the untouched original, and measures noise-only and speech regions before exploring spectral noise reduction.

## Quality gates

Every audio feature needs automated tests for correctness and failure paths plus listening tests on representative voices, rooms, microphones, and background noise. Avoid claiming that ordinary filters remove severe room echo or noise; those problems may require more advanced algorithms or models.

## Non-destructive noise-gate export (initial Android workflow)

The Android shell offers a document picker for a WAV file and processes it off the UI thread using the shared C++ reader, conservative noise gate, and WAV writer. Processing reports frame-based percentage progress and can be cancelled between bounded audio blocks; cancellation removes the incomplete processed output and leaves the selected source untouched. Copying the source into private staging is also cancellable. A successful operation exports a **separate** file to `Music/Audio Studio/Processed`; it never writes over the selected source. The processing path accepts valid PCM16 mono/stereo WAV files and streams audio in fixed 4,096-frame blocks, so processing memory no longer scales with recording duration. Standard RIFF/WAV size limits still apply, and Android temporarily stages the source and processed comparison files on disk. After processing, the app retains one private original/processed pair for in-app A/B listening, with Play original, Play noise-reduced copy, and Stop playback controls. Processing another file replaces that temporary comparison pair; the exported processed copy remains in Music.

The gate is intentionally conservative and affects quiet sections more than loud speech. It is not spectral or AI noise cancellation, does not remove room echo, and may not help when noise overlaps speech. The UI path requires Android 10+ for the public MediaStore export in this first implementation. Before considering this production-ready, install the CI APK and listen to both the original and processed versions on real speech with quiet gaps and real background noise. Check for pumping, clipped word endings, and speech that becomes too quiet.
