# Android and Windows Strategy

## Shared product, different operating environments

Both Android and Windows are first-class targets. Shared project models, processing settings, workflows, and most UI logic should remain common. Microphone APIs, permissions, lifecycle, routing, and interface density must respect each platform.

## Android

Considerations:
- Runtime microphone permission and clear permission-denied recovery.
- Audio focus/interruption and phone-call or other-app interruptions.
- App backgrounding, screen locking, battery use, and recording lifecycle.
- Device route changes such as wired, Bluetooth, and built-in microphones.
- Scoped storage and user-visible export destinations.
- Small-screen controls suitable for starting/stopping a take safely.
- Supported device/OS variation and codec availability.

No background recording should happen without a clear user-initiated session and platform-compliant disclosure/permissions.

## Windows

Considerations:
- Input-device selection and default-device changes.
- Microphone privacy settings and permission errors.
- Device sample rates, channel layouts, and buffer behavior.
- Long-form recording, keyboard shortcuts, precise timeline interaction, and multi-window/resizable layouts.
- User-selected project/export folders and disk-space checks.
- Robust behavior when an input device is unplugged during recording.

## Shared behavior

- Projects and original recordings remain local by default.
- Display recording status and input level clearly.
- Report interruptions and incomplete files.
- Do not silently change input devices or processing settings without feedback.
- Make processing state explicit: original, processed preview, or final render.
- Provide clear exports that can be used in common video editors.

## Verification matrix

Test at least one real Android device and one Windows machine before declaring cross-platform milestones complete. Automated builds are useful but do not replace real microphone tests. Track platform-specific defects separately and do not claim Windows or Android readiness solely from a shared Dart test passing.
