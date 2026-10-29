# Settings preference contract

Status: design contract only. This document does not add a Settings screen or change audio-processing behavior.

## Purpose

Settings should expose only preferences that genuinely need to survive app restarts. Each preference must have a clear owner, a default, a persistence scope, validation rules, and a reset behavior. Controls that belong to a workflow should remain in that workflow; a Settings screen must not duplicate them.

## Current implementation audit

### Android

The app uses the private SharedPreferences file `noise_gate_preferences` for four processing parameters and the selected preset:

| Key | Meaning | Stored value | Current fallback |
| --- | --- | --- | --- |
| `threshold_progress` | Noise threshold slider position (0–50; displayed as position − 70 dBFS) | Integer | 22 (−48 dBFS) |
| `attenuation_progress` | Attenuation slider position (0–36; displayed as position − 36 dB) | Integer | 18 (−18 dB) |
| `dsp_gain_progress` | Output gain slider position (0–36; displayed as position − 24 dB) | Integer | 24 (0 dB) |
| `high_pass_progress` | High-pass position (0 = Off; other positions map to 20–500 Hz) | Integer | 0 (Off) |
| `selected_preset` | Last selected preset label | String | `Natural` |

The four slider values are persisted when the user releases a slider; applying a preset saves all four positions and the preset label. Values are clamped to the slider ranges when loaded. These preferences are app-private and normally survive process/app restarts, but are removed when app data is cleared or the app is uninstalled.

**Known mismatch to resolve before exposing these preferences in Settings:** a fresh install visually selects `Natural`, but the slider fallback values (−48 dBFS, −18 dB) do not match the `Natural` preset values (−54 dBFS, −12 dB). The selected preset marker must never imply that the current slider values are the preset values when they differ.

### Windows

Processing sliders and the preset ComboBox currently use XAML defaults and are not saved as durable user preferences. Preset selection updates the sliders and status, but does not persist the chosen preset or custom values across app restarts. Windows therefore does not yet have persistence parity with Android.

## Contract for the first cross-platform preference set

The initial durable preference set should be limited to the existing processing configuration:

1. **Noise threshold** — numeric value in the existing supported range, with the same unit and limits on both platforms.
2. **Noise attenuation** — numeric value in the existing supported range, with the same unit and limits on both platforms.
3. **Output gain** — numeric value in the existing supported range, with the same unit and limits on both platforms.
4. **High-pass filter** — Off or one of the existing supported cutoff values, with the same mapping on both platforms.
5. **Selected preset label** — Natural, Voice clarity, or Rumble cut. This label describes the last applied preset, not a guarantee that the user has not subsequently customized its values.

### Defaults

Use the existing **Natural preset** as the single fresh-install default on both platforms:

- Threshold: −54 dBFS.
- Attenuation: −12 dB.
- Output gain: 0 dB.
- High-pass: Off.
- Selected preset: Natural.

Before adopting these as defaults, preserve the current processing algorithm and verify that the Natural preset's displayed labels and actual slider values agree. Do not silently rewrite existing saved user values during migration.

### Persistence and validation

- Save the four parameter values when a slider interaction completes, not on every movement event.
- Persist the selected preset only when the user explicitly applies a preset.
- Custom slider edits do not silently change the selected preset label; the label remains the last applied preset, and the sliders are the source of truth for the actual processing configuration.
- Validate/clamp persisted values against the existing control ranges at load time. Unknown preset labels fall back to Natural for display, without discarding valid custom parameter values.
- Use app-local storage on each platform. No account, cloud sync, or cross-device sync is in scope.
- Keep preference reads/writes off expensive audio-processing paths; processing must use a snapshot of the values chosen when the user starts the operation.
- A future Settings screen may show the current processing configuration and provide **Reset processing settings**. Reset must restore the defaults above and the Natural selection, and require an explicit user action. It must not clear recordings, exported WAVs, library folders, diagnostics, or unrelated app state.

## Explicitly out of scope for Settings v1

- Default output location: do not expose until both platforms support a stable, permission-safe, user-visible destination and the recording/trim/process/export flows all honor it.
- Microphone/input selection: keep in the Record workflow unless a durable cross-platform device-selection contract is designed.
- Library folder: Windows folder choice is workflow/discovery state today; Android's library scope is fixed to Audio Studio's WAV location. Do not pretend they are equivalent settings.
- UI theme, autosave toggles, delete/rename controls, cloud backup, and DSP-engine options: no settings UI until the feature exists, has safe semantics, and is tested.

## Implementation order

1. Align fresh-install processing defaults with the Natural preset without changing the DSP algorithm.
2. Add Windows app-local persistence for the same four values and selected preset, with validation and safe load/save behavior.
3. Add focused tests for defaults, range validation, persistence across restart, unknown preset labels, reset behavior, and ensuring processing captures a stable parameter snapshot.
4. Only after cross-platform behavior is verified, add a small Settings surface for reviewing/resetting processing preferences. Avoid duplicating sliders already visible in the Processing workflow.
5. Keep the Reset action narrowly scoped and test that audio files and library state remain untouched.

## Validation gate

- Android native shell/APK, Linux C++ core, and Windows C++ core/app shell CI must all pass.
- Manual validation on both platforms must confirm values survive app restart, preset/custom values remain internally consistent, reset restores Natural defaults, and recordings/processed/trimmed WAVs are not touched.
- CI success does not replace Windows runtime validation. The Windows persistence/reset path remains unverified until it can be exercised in the Windows app.
