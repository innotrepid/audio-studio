# Android and Windows Strategy

## Shared product, native platform behavior

Audio Studio targets Android and Windows using native user interfaces and a shared C++ audio core. Product concepts and terminology should be consistent across platforms, while layouts, lifecycle behavior, file access, and audio-device integration remain platform-specific.

## Android

- Kotlin UI and application lifecycle.
- Android NDK for C++ integration.
- Oboe for native audio capture/playback integration.
- Handle microphone permission, audio focus, calls/interruption, backgrounding, device changes, and interrupted recording recovery.
- Keep recording and processing work off the UI thread and real-time audio callback.
- Use app-controlled storage for internal working files and explicit user-facing destinations for exports.
- Design for touch-first portrait workflows; use landscape only where an existing workflow explicitly benefits and remains usable if orientation cannot be changed.
- Test across real devices because input routes, latency, and performance vary by manufacturer.

## Windows

- C# with WinUI 3 for a desktop-first interface.
- Shared native C++ core through a narrow C ABI boundary.
- Native Windows audio APIs, including WASAPI where appropriate, for capture/playback and device/endpoint handling.
- Support mouse, keyboard, accessible controls, file-system workflows, scalable windows, and wider editing layouts.
- Test device removal, sleep/resume, permissions, recording recovery, and packaged/unpackaged launch behavior.

## Shared components

Keep platform-independent behavior in C++ where appropriate:
- Audio formats, buffers, WAV parsing/decoding, and validation.
- DSP primitives and offline processing.
- Waveform analysis and frame-based audio operations.
- Deterministic processing contracts and regression tests.

Keep these platform-specific:
- Microphone capture, device routing, permissions, audio focus, and lifecycle.
- UI, file pickers, background execution, storage access, and OS integration.
- Platform playback/session management where the underlying APIs differ.

## Current application and CI state

The repository already contains native Android and Windows application shells and a shared C++ core. Android uses Kotlin/NDK/Oboe; Windows uses WinUI 3 and a native bridge. CI includes Android build validation and Linux/Windows C++ core checks, with Windows application build validation where configured.

A green C++ core build does not prove microphone behavior, audio quality, or full desktop runtime behavior. Keep physical-device Android checks and Windows runtime checks as separate release gates.

## UI direction

Use the Hybrid Studio model:
- Record and Creator workflows should be calm, approachable, and task-oriented.
- Editor and processing workflows should expose precise controls without overwhelming the user.
- Android uses touch-first bottom navigation; Windows uses a persistent left navigation rail and wider work area.
- Both platforms share labels, status meanings, visual tokens, and interaction principles without forcing identical layouts.
- Primary interactive accent: blue. Mint is reserved for live/healthy audio states; warm colors communicate warnings and errors.
