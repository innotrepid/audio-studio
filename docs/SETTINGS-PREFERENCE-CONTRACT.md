# Settings preference contract

Status: processing preference persistence is implemented; a dedicated Settings screen remains gated on focused automated tests.

## Scope

The first durable preference set is the existing processing configuration only:

- Noise threshold in dBFS.
- Noise attenuation in dB.
- Output gain in dB.
- High-pass position (Off or an existing cutoff).
- Last explicitly applied preset label: Natural, Voice clarity, or Rumble cut.

No account, cloud sync, cross-device sync, microphone selection, library folder, output location, theme, delete/rename, or DSP-engine options are included.

## Defaults and units

Fresh installs on both platforms use the Natural preset values:

- Threshold: −54 dBFS.
- Attenuation: −12 dB.
- Output gain: 0 dB.
- High-pass: Off.
- Selected preset: Natural.

Supported ranges are aligned across platforms: threshold −70 to −20 dBFS, attenuation −36 to 0 dB, output gain −24 to +12 dB, and high-pass slider position 0–49 (0 means Off; other positions map to 20–500 Hz).

Android retains its existing private SharedPreferences file and keys. Only missing-key fallback values changed; existing stored user values are not migrated or overwritten.

Windows stores preferences in `%LOCALAPPDATA%/AudioStudio/processing-preferences.json`, outside the WAV/library folders. Writes use a temporary file followed by replacement of that settings file. This is intentionally app-local persistence for the unpackaged Windows application.

## Restore, validation, and save behavior

- Clamp persisted numeric values to supported ranges on load.
- Malformed or inaccessible Windows preference files fall back to safe defaults.
- Unknown preset labels display as Natural without discarding otherwise valid custom slider values.
- Persist slider values after pointer-drag completion or keyboard interaction completion, not on every movement.
- Applying a preset explicitly saves its label and parameter values. Custom edits keep the last applied preset label; slider values remain the source of truth for processing.
- Processing passes the chosen values to the native engine at operation start; changing controls later does not mutate an already-started operation.
- No preferences are synced or written into audio folders.

## Reset contract for the future Settings surface

Reset must restore the four Natural values and Natural label through an explicit user action. It must not delete or alter recordings, processed/trimmed WAVs, library folder selections, or diagnostics.

## Validation gate before a Settings screen

Add focused automated coverage for fresh defaults, numeric clamping, persistence across restart, unknown preset labels preserving custom values, reset isolation, and stable processing snapshots. Then add a minimal Settings surface for reviewing/resetting preferences without duplicating Processing sliders. CI must pass for Android APK, Linux C++ core, and Windows core/app shell. Manual runtime checks on Android and Windows remain separate from CI; Windows runtime testing is still pending the user's Windows environment.
