# Audio Studio — Screen Architecture

## Goal

Replace the current long, control-heavy screen with a calm, task-oriented interface without changing the reliable audio engine underneath it. Recording, review, cleanup, editing, and export should each have a clear home. Keep the original recording safe and make the next action obvious.

## Navigation model

Use the same product concepts on Android and Windows, but adapt the navigation to each device:

- **Android:** four-item bottom navigation — Record, Library, Edit, Settings. Keep the primary record/stop action prominent on the Record screen. Avoid squeezing desktop-style panels onto a phone.
- **Windows:** a persistent left navigation rail — Record, Library, Editor, Processing, Settings — with a wider work area and room for side-by-side comparison.
- Use a shared project/file model and shared labels, while allowing platform-native controls and layouts.
- Do not introduce a separate dashboard/home screen yet; open directly into Record, with a compact recent-recordings shortcut.

## Screen 1 — Record

**Purpose:** Start a recording quickly and confidently monitor input.

- Microphone/input-device selector and refresh action.
- Large, unmistakable Record / Stop control and elapsed timer.
- Live RMS/input level meter and peak/clipping warning, which already exist and must be preserved.
- Clear recording state and useful permission/device/storage errors.
- After stopping, show a concise saved confirmation and actions to play, rename/manage, or open the take in Edit.
- Recovery for interrupted recordings belongs here but should be secondary, not a dominant panel.

**Do not put** all noise-processing sliders, waveform trim controls, and A/B file pickers on this screen.

## Screen 2 — Library

**Purpose:** Find and manage recordings and generated versions.

- List recordings with filename/title, duration, date, and status.
- Distinguish original recordings, processed copies, and trimmed exports.
- Search/sort when the list grows; start with a simple, fast list.
- Per-item actions: play, open in Editor, process, rename, and delete only with clear confirmation.
- Make source-versus-derived relationships clear; never silently replace the original.
- Empty state should guide the user to record or import a WAV.

A richer project/take organization can come later; do not block the first UI redesign on a database or complex project manager.

## Screen 3 — Editor

**Purpose:** Inspect a recording and make safe, precise edits.

- Waveform occupies the main visual area.
- Selection range has clear start/end handles or markers, selected-region shading, and precise start/end/duration readout.
- Actions close to the selection: Preview selection, Trim to new WAV, and reset/full-range selection.
- Show source filename and an explicit “original unchanged” message.
- Export/save progress and errors should be visible without losing the selection.
- On Windows, support a wider waveform and room for file details. On Android, prioritize touch targets and vertical scrolling.

The existing waveform analysis, selected-range preview, and non-destructive trim are the foundation. Do not redesign the DSP engine as part of this UI work.

## Screen 4 — Processing

**Purpose:** Apply voice cleanup without overwhelming the user.

- Start with three clear presets: Natural, Voice clarity, and Rumble cut.
- Put advanced controls (noise threshold, attenuation, gain, high-pass) inside an “Advanced controls” expandable section.
- Show a short plain-language explanation of the selected preset and that results vary by recording.
- Process to a new output file; never overwrite the original.
- Progress, cancellation, and completion/error status must be easy to understand.
- Provide Original / Processed A/B playback with a single obvious switch and stop action.
- Android may keep processing entry points accessible from Edit/Library; Windows can have a dedicated Processing page.

Avoid claiming studio-grade AI noise cancellation. The current gate and DSP effects are local tools with known limits.

## Screen 5 — Settings

**Purpose:** Keep infrequent configuration out of the main workflow.

- Audio input and recording defaults where platform APIs support them.
- Output/file location guidance and recording format details.
- Processing defaults/preset behavior.
- About, limitations, and troubleshooting.
- Platform-specific permission/device guidance.

Do not duplicate the live input meter here; it belongs on Record.

## Later screens — only when the foundations are ready

### Projects and scripts
A project can eventually group scripts, takes, selected/approved takes, and final narration. Add this only alongside autosave, recovery, and a clear project-file format.

### Script / teleprompter
Script editor, version history, section markers, adjustable teleprompter, and recording linked to a script section. This is a later Creator Studio milestone, not a prerequisite for modernizing the current app.

### Transcript and captions
Transcript-assisted selection, reviewable silence/filler suggestions, editable captions, and SRT/VTT export should arrive only when transcription and timing are dependable. Never silently remove spoken content.

### Session quality
Comparisons of loudness, noise, and spectral balance across sessions should be presented as suggestions, with measurements and user approval.

## Visual direction

- Modern, restrained creator-tool aesthetic: strong hierarchy, generous spacing, readable type, restrained accent color, and consistent iconography.
- One primary action per screen; secondary actions should not compete with it.
- Use cards only to group related tasks, not to wrap every control in a box.
- Prefer waveform and clear status indicators over decorative graphics.
- Consistent spacing, button sizes, field styling, loading/empty/error states, and accessible contrast.
- Android should feel touch-first; Windows should feel like a desktop production tool, not a stretched phone UI.
- Preserve native platform conventions where they improve usability. Avoid introducing a cross-platform UI framework.

## Implementation sequence

1. **UI foundation:** navigation shell, shared colors/type/spacing, reusable status and action components. No audio-engine changes.
2. **Record screen:** move existing recording controls and live RMS/peak/clipping indicators into a focused layout.
3. **Library:** establish a clear list of originals and derived outputs.
4. **Editor:** move waveform, range selection, precise timing, preview, and trim into one coherent workflow.
5. **Processing:** presets first, advanced controls collapsed by default, then simple A/B comparison.
6. **Settings and polish:** empty/loading/error states, accessibility, and platform-specific layout refinement.
7. **Later product phases:** projects/scripts, teleprompter, transcript/captions, and session intelligence.

## Acceptance criteria for the redesign

- Existing recording, save, recovery, processing, playback, waveform, preview, and trim behavior remains intact.
- Live RMS/peak monitoring and clipping warnings remain visible during recording.
- A first-time user can identify how to record, find a recording, preview a trim, and export a processed copy without being confronted by every advanced control.
- The original recording is never overwritten by processing or trimming.
- Android and Windows share the same concepts but are laid out appropriately for their screen sizes.
- All three CI jobs pass, followed by physical-device/manual checks for actual usability and audio behavior.
