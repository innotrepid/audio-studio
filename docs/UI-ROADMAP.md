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

## UI performance contract: update only what changed

This is a product-level requirement for every screen and interaction. A tap or gesture must not trigger a full screen rebuild, full data reload, or expensive work unless the operation genuinely requires it.

- **Keep screen instances alive.** On Android, build the view hierarchy once and update existing views; on Windows, switch existing workflow panels and preserve their controls/state. Do not recreate the activity/window or replace the full content tree to update a label, slider, selection, progress value, or playback state.
- **Use the smallest update scope.** Update the affected text, enabled state, progress indicator, selected range, or custom view only. For waveform trim gestures, redraw the waveform view with cached peak data; never re-read or re-analyze the WAV while the user drags a selection.
- **Separate data work from rendering.** File I/O, waveform analysis, and native processing must not run synchronously on the UI thread. Show operation state and progress without repeatedly reconstructing the page.
- **Keep gesture handlers cheap.** Slider/drag handlers should only update in-memory selection and the relevant labels/visual overlay. Persist preferences only when appropriate, and debounce/throttle expensive work rather than doing it on every movement.
- **Do not poll unrelated state.** Timers should update only the live meter, elapsed time, or active processing progress. Stop timers when the operation ends or the screen is destroyed.
- **Preserve navigation state.** Switching Record/Edit/Process/Compare should not discard loaded waveform peaks, selected files, slider values, or processing configuration. Do not reinitialize audio resources merely because a panel becomes visible.
- **No blanket refresh APIs.** Avoid calling setContentView again, rebuilding all child views, reloading the library, or forcing global invalidation/layout as a shortcut for local state changes.
- **Prove it with interaction checks.** Verify that repeated taps, slider drags, navigation, and scrolling do not reset unrelated values, reload files, restart audio resources, or cause visible flicker. Check UI responsiveness while waveform analysis and processing run.

### Current architecture observations
- Android currently constructs its screen hierarchy in MainActivity.onCreate() and calls setContentView(appShell) once. WaveformView.setSelection() updates its selection state and invalidates only that custom view; it uses cached peaks for drawing.
- Windows currently constructs the workflow panels in XAML once. ShowWorkflowPanel() toggles their visibility rather than rebuilding the window.
- These are useful foundations, not proof that every path is optimal. Preserve them and add targeted profiling/tests before any architecture change.

## Delivery phases

1. **Navigation foundation:** completed in PR #68; connect existing workflows without duplicating audio state.
2. **Visual hierarchy and reusable styling:** improve spacing, headers, cards, button hierarchy, and status presentation without changing audio behavior.
3. **Record screen polish:** make input meter, recording state, clipping, and recovery easier to read.
4. **Edit screen polish:** improve waveform sizing, selection feedback, and trim confirmation.
5. **Process and Compare polish:** make presets, advanced controls, progress/cancel, and A/B preview clearer.
6. **Incremental-update hardening:** audit interaction handlers and add targeted checks so routine taps/gestures update only the affected controls.
7. **Library and Settings:** design and implement only after requirements and persistence behavior are agreed.

## Safety and validation gates

- Do not alter native DSP or recording behavior as part of visual-only changes.
- Never overwrite original recordings during trim or processing.
- Keep cancellation and resource cleanup behavior intact.
- Require all three CI jobs to pass: Android native shell/APK, Linux C++ core, and Windows C++ core.
- After visual phases, run focused manual smoke tests on each available platform; CI alone cannot validate visual quality, interaction smoothness, or microphone behavior.


## Visual direction update — Resonate-inspired, not glass-heavy

Use the visual language of the Resonate `Resonate-True-Apk` branch as a reference, not as a UI to copy literally:

- Deep near-black / charcoal foundation with layered, **opaque** surfaces; avoid applying blur or glass effects to every card.
- Violet as the main interactive accent, pink/orange as rare secondary highlights, and mint reserved for live recording/healthy signal states.
- Strong typographic hierarchy, generous but efficient spacing, confident waveform scale, and clearly differentiated primary versus secondary actions.
- Keep the studio visually distinctive and finished: purposeful status treatments, subtle borders, consistent corner radii, and clear control grouping rather than generic Windows utility styling.
- Edit and Process on Android should request sensor landscape when selected; Record and other workflows should release that request. The same activity and existing views must remain alive through orientation changes. If the system cannot honor the request, all controls must remain usable.
- Preserve incremental updates: changing a trim handle should update the selection and summary only; slider drags should update their value labels; neither should reload the file or rebuild the screen.

This visual direction supersedes the earlier mint-only visual treatment as the long-term target. Keep the visual redesign in small reviewable slices and do not change recording/DSP semantics just to style the interface.

### Navigation affordances follow-up

- Windows workflow navigation uses familiar glyph icons alongside concise labels, with a high-contrast selected state so the active workspace is obvious.
- Navigation updates only the existing navigation controls and toggles the already-created workflow panels; it does not reconstruct the content tree or reset editor/processing state.
- Icon glyphs are decorative cues, not replacements for accessible text labels.


## Record screen UI slice — 2026-10-29

### One-button recording interaction
- Replace separate Start and Stop buttons with one primary toggle action on both Android and Windows.
- Idle state: blue button labelled **Start recording**.
- Active state: teal-green button labelled **Stop & save WAV**.
- Disable the control only while a file picker, processing operation, or finalization makes a new action unsafe. During recording, keep the single button enabled so it can stop and save.
- Keep the existing elapsed timer, live RMS/peak meter, clipping warning, microphone selection, and recovery flow intact.
- Use accessible names in addition to icon-like visual cues; color must not be the only recording-state signal.
- Do not change native capture, WAV validation/publication, recovery, or DSP semantics.

### Record screen hierarchy
- Recording state and elapsed time first, then the input meter/clipping state.
- One large primary action with blue idle / teal active colors; use charcoal surfaces and concise labels.
- Microphone selection and refresh stay visible but visually secondary.
- Recovery remains available without competing with the primary recording action.

### Validation gates
- CI must pass for Android, Linux native core, and Windows native shell/core.
- Manual smoke checks: start, observe teal state and timer/meter, stop and save, verify blue idle state and WAV playback; repeat twice; cancel the save picker; check failure/recovery paths.
- Windows runtime validation remains pending the user's Windows environment being ready.

