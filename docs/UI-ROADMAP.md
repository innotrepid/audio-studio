# Audio Studio UI roadmap

## Product direction

Audio Studio should feel like a focused voice-production tool, not a collection of debug controls. Keep the native audio engine and file-safety behavior stable while progressively improving navigation, hierarchy, and feedback on Android and Windows.

## Screen map

### 1. Record — primary landing screen
- Microphone/input selector and refresh action.
- Clear input-level meter, peak/clipping warning, and recording state.
- One dominant Start/Stop action whose enabled state reflects the native engine.
- Recording destination and interrupted-recording recovery remain discoverable without competing with the main action.

### 2. Edit — waveform and trim
- File selection and waveform preview.
- Start/end selection with readable timestamps and selected duration.
- Export trimmed audio to a new file; always communicate that the source is preserved.
- Keep trim validation visible and prevent invalid ranges.

### 3. Process — noise reduction
- Preset-first workflow: Natural, Voice clarity, Rumble cut.
- Advanced DSP sliders collapsed by default.
- Progress, cancellation, and failure states grouped near the primary Process action.
- Explain that processing creates a separate output and does not overwrite the source.

### 4. Compare — original vs processed
- Two clearly labelled file slots.
- Obvious Original / Processed playback actions and one Stop action.
- Show which file is playing and recover gracefully if preview cannot start.
- Future enhancement: matched-position A/B switching, only when playback state can be kept reliable.

### 5. Library — later phase, real functionality only
- Add only after recording discovery, sorting, deletion/rename safeguards, and file permissions are designed and tested.
- Do not show a placeholder tab that implies a usable library exists.

### 6. Settings — later phase, real functionality only
- Add only when there are meaningful persisted preferences (for example, default output folder or preferred processing preset).
- Avoid a settings screen that only duplicates existing controls.

## Visual system

- Dark studio surface with a restrained accent colour; use high contrast for status and warnings.
- One consistent page header, card spacing, button hierarchy, control sizing, and status typography.
- Primary actions should be visually dominant; secondary actions should be quieter.
- Prefer concise, user-facing status text over raw engine terminology except in diagnostic details.
- Preserve platform conventions: bottom navigation on Android, adaptive navigation/tabs on Windows.
- Support narrow windows, larger text, keyboard navigation on Windows, and scrolling on small screens.

## Delivery phases

1. **Navigation foundation:** completed in PR #68; connect existing workflows without duplicating audio state.
2. **Visual hierarchy and reusable styling:** improve spacing, headers, cards, button hierarchy, and status presentation without changing audio behavior.
3. **Record screen polish:** make input meter, recording state, clipping, and recovery easier to read.
4. **Edit screen polish:** improve waveform sizing, selection feedback, and trim confirmation.
5. **Process and Compare polish:** make presets, advanced controls, progress/cancel, and A/B preview clearer.
6. **Library and Settings:** design and implement only after requirements and persistence behavior are agreed.

## Safety and validation gates

- Do not alter native DSP or recording behavior as part of visual-only changes.
- Never overwrite original recordings during trim or processing.
- Keep cancellation and resource cleanup behavior intact.
- Require all three CI jobs to pass: Android native shell/APK, Linux C++ core, and Windows C++ core.
- After visual phases, run focused manual smoke tests on each available platform; CI alone cannot validate visual quality or microphone behavior.
