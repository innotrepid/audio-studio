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


## UI research and implementation direction — 2026-10-29

The dedicated UI brainstorm has resumed. Research references include Audacity's pared-back Modern workspace and persistent level-meter panel, Adobe Podcast's task-oriented record/enhance/review flow, and REAPER's efficient but highly customizable desktop controls. Audio Studio should borrow useful interaction patterns without copying any product's layout.

### Our angle: a focused voice-production studio
- **Record first:** open directly into recording; keep the record/stop action, elapsed time, live RMS/peak, clipping warning, and input selection easy to scan.
- **Waveform-led editing:** give the waveform the most visual space; keep trim handles, exact timecodes, selection duration, preview, and non-destructive export together.
- **Preset-first processing:** Natural, Voice Clarity, and Rumble Cut first; advanced parameters remain secondary/collapsed. Make original-versus-processed review obvious.
- **Icon-led, not icon-only:** use familiar icons with short labels in primary navigation. Use accessible names/tooltips for icon-only utility actions; never make critical actions ambiguous.
- **Dark, opaque studio surfaces:** near-black/charcoal foundation, violet interaction accent, mint for healthy/live signal, and warm warning colors only for warnings. Avoid pervasive blur/glass, excessive card borders, and decorative visualizers.
- **Responsive by platform:** Android uses a touch-friendly bottom bar and vertical content; Windows uses a compact persistent left rail and a wider work area.
- **Fast-feeling interactions:** keep view instances alive, preserve selection and processing settings across navigation, and update only the controls affected by an interaction.

### Delivery slices
1. **Navigation foundation:** icon-led Android bottom navigation with selected-workspace feedback and accessible labels; retain existing screen instances and behavior.
2. **Shared visual hierarchy:** consistent screen headers, spacing, surface levels, typography, status chips, and primary/secondary action styles.
3. **Record focus:** clearer recording state, meter/clipping presentation, and post-save actions.
4. **Editor focus:** waveform scale, range controls, timecodes, preview, and safe-export confirmation.
5. **Processing focus:** preset emphasis, collapsed advanced controls, clear progress/cancel, and A/B review.
6. **Library and Settings:** keep Library grounded in its real file discovery behavior; add Settings only when each preference has a clear persistence contract.

### Validation
- Preserve recording, save/recovery, playback, waveform analysis, trim, processing, cancellation, and no-overwrite behavior.
- No audio-engine or DSP semantic changes in visual-only slices.
- All three CI jobs must pass. Manual checks should include repeated navigation, selected-state feedback, preserved trim/settings state, recording meter updates, small-screen layout, and keyboard/focus accessibility on Windows.
- CI success is not proof of visual usability or actual audio quality; platform-specific manual checks remain separate.


## Record screen UI slice — 2026-10-29

### One-button recording interaction
- Replace separate Start and Stop buttons with one primary toggle action on both Android and Windows.
- Idle state: blue button labelled **Start recording**; active state: teal-green button labelled **Stop & save WAV**.
- Disable the control only while a file picker, processing operation, or finalization makes a new action unsafe. During recording, keep the single button enabled so it can stop and save.
- Keep elapsed time, live RMS/peak meter, clipping warning, microphone selection, and recovery flow intact.
- Use accessible names alongside icon cues; color must not be the only recording-state signal.
- Do not change native capture, WAV validation/publication, recovery, or DSP semantics.

### Validation gates
- CI must pass for Android, Linux native core, and Windows native core/shell.
- Manual smoke checks: start, observe teal state and timer/meter, stop and save, verify blue idle state and WAV playback; repeat twice; cancel the save picker; check failure/recovery paths.
- Windows runtime validation remains pending the user's Windows environment being ready.

## Editor waveform focus — 2026-10-29

- Give the Android waveform more vertical space and make selected duration the most visible piece of trim feedback.
- Keep the in/out timecodes together in one compact row while retaining readable time formats and a full-width apply action.
- Keep preview and export as distinct actions; exporting creates a new WAV and preserves the source.
- Keep waveform peak analysis cached; dragging handles updates selection feedback only and must not re-read or re-analyze the file.
- No native audio or trim semantics changed. Manual validation: load a WAV, drag both handles, apply exact timecodes, preview the selection, export a trim, and verify the source remains unchanged.
