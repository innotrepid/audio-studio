# Android and Windows Strategy

## Shared product, native platform behavior

Audio Studio targets Android and Windows without requiring one UI toolkit to control both. The shared C++ core minimizes duplicated audio logic, while native shells handle each platform's lifecycle and interaction patterns.

## Android

- Kotlin UI and application lifecycle.
- Android NDK for C++ integration.
- Evaluate Google's Oboe for native low-latency audio input/output.
- Request microphone permission transparently and handle denial or revocation.
- Handle calls, audio focus changes, app backgrounding, device changes, and interrupted sessions.
- Keep recordings and project metadata in app-controlled storage by default; make export destinations explicit.
- Test across real devices because audio latency and available input routes vary by manufacturer.

## Windows

- C# with WinUI 3 for a desktop-first interface.
- Shared native C++ core through a narrow, documented interop boundary.
- Use Windows audio APIs (including WASAPI where appropriate) for capture/playback and handle device changes, endpoint selection, and format negotiation.
- Support mouse, keyboard, accessible controls, file-system workflows, and large project views.
- Test microphone permissions, device removal, sleep/resume, and recording recovery.

## Shared components

Keep these in C++ where they are genuinely platform-independent:
- Audio formats, buffers, and validation.
- DSP primitives and offline processing.
- Shared project/audio data rules.
- Quality metrics and deterministic processing tests.

Keep these platform-specific:
- Microphone capture, device routing, permissions, focus, and lifecycle.
- UI, file pickers, background execution, and OS integration.

## CI and release strategy

The first CI milestone compiles and tests the shared C++ core on Linux and Windows. Later CI milestones add Android NDK/Gradle builds and Windows app packaging once each native shell exists. A green core build must not be described as proof that microphone capture works on either platform.
