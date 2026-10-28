# Android application

Audio Studio's Android shell uses Kotlin for the UI and lifecycle/permissions, the Android NDK for native integration, and the shared C++ audio engine. Oboe is included for native audio capture integration.

## Build

Open this folder in Android Studio with Android SDK 35, JDK 17, CMake 3.22.1, and Android NDK installed. CI builds a debug APK.

## Implemented capabilities

- Record microphone audio and save finalized PCM16 WAV recordings.
- Save recordings in the user-accessible `Music/audio studio/recordings` folder.
- Show recording elapsed time and provide explicit start/stop-and-save controls.
- Process a selected WAV into a separate output file using the current conservative noise-gate/attenuation processing path.
- Adjust the noise-gate threshold and attenuation settings.
- A/B playback of the original and latest processed WAV pair.
- Process files in bounded blocks, report progress, and allow cancellation between blocks so the app does not need to decode an entire file into memory.

The recording/save path and basic processing workflow have been exercised on a physical Android device. That is a useful validation milestone, not a guarantee for every device, microphone, or Android version.

## Current limitations

- The current noise-reduction implementation is a conservative noise gate/attenuation foundation, not AI noise cancellation or full spectral denoising. Results depend on the voice, background noise, and selected settings.
- Physical-device listening tests are still needed across more environments, including quiet speech gaps and real background noise. Compare original and processed audio before relying on a take.
- Device-specific interruptions, microphone routing changes, storage exhaustion, and long-session behavior still need broader hardware validation.
- The Android CI job proves that the native shell/APK builds; it does not by itself prove microphone hardware behavior or audio quality.

## Reliability principles

- Keep original recordings intact; processing writes a separate output.
- Do not treat an incomplete recording as a valid finalized WAV.
- Preserve recoverable partial data after capture failures where supported, and validate it before recovery.
- Keep file I/O and processing off real-time audio callbacks.

See the repository [roadmap](../../docs/ROADMAP.md) and [architecture](../../docs/ARCHITECTURE.md) for planned work and design constraints.
