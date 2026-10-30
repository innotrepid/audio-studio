# Audio Studio UI Roadmap

## Product direction

Audio Studio uses a **Hybrid Studio** experience: approachable Record and Creator workflows alongside precise Editor and Processing workspaces. Keep the native audio engine and file-safety behavior stable while improving navigation, hierarchy, and feedback.

### Visual system

- Deep near-black/charcoal foundation with layered, opaque surfaces.
- **Blue** is the primary interactive accent for selected navigation, primary actions, and active editing tools.
- Mint is reserved for live recording and healthy signal states.
- Warm warning/error colors are semantic and used sparingly.
- Strong typographic hierarchy, generous but efficient spacing, confident waveform scale, subtle borders, and consistent corner radii.
- Avoid pervasive blur/glass, excessive card borders, and decorative visualizers.
- Use icons with concise labels; icon-only utility actions require accessible names/tooltips.

## Navigation

Four primary destinations:
1. Record
2. Editor
3. Creator
4. Library

Settings remains secondary. Processing presets, advanced controls, and original/processed comparison belong within the Editor workflow.

### Android

- Touch-friendly four-item bottom navigation.
- Vertical content layout and comfortable tap targets.
- Creator uses focused sub-navigation rather than one long page.
- Preserve screen instances and state across navigation.

### Windows

- Persistent left navigation rail with icon and text labels.
- Wider work area for waveform editing and comparison.
- Keyboard navigation, visible focus, and adaptive layouts.

## Workspace goals

### Record

- One dominant Start/Stop action.
- Input selection, live RMS/peak meter, clipping warning, elapsed time, and recovery guidance.
- Clear post-save actions.
- Do not place advanced processing or trim controls on the Record screen.

### Editor

- Waveform-led editing with precise range/timecode feedback.
- Preview selection and non-destructive export remain distinct.
- Preset-first processing; advanced controls collapsed by default.
- Clear original-versus-derived identity and A/B review.
- Progress, cancellation, and error feedback without losing current selection.

### Creator

- Scripts: section-based editor, notes, cues, autosave.
- Teleprompter: readable mode, adjustable pace/typography/spacing, manual and automatic scrolling.
- Transcripts & Captions: editable segments, explicit timing provenance, SRT/WebVTT export.
- Production: markers, retake notes, review and approval states.

### Library

- Clear source/derived file categories and safe next actions.
- Search filters loaded entries; it must not trigger a storage rescan.
- Preserve platform-specific discovery boundaries and actionable empty/error states.

## UI performance contract: update only what changed

- Keep screen instances alive; do not recreate the activity/window or replace the full content tree for routine updates.
- Update only the affected text, enabled state, progress, selection overlay, or custom view.
- Waveform trim gestures use cached peak data; never re-read or re-analyze the WAV while dragging.
- File I/O, waveform analysis, and native processing stay off the UI thread.
- Gesture handlers remain cheap and update in-memory state/labels only.
- Persist preferences at appropriate interaction boundaries, not on every slider movement.
- Timers update only the active meter, elapsed time, or processing progress and stop when no longer needed.
- Navigation preserves loaded files, waveform peaks, trim selection, and processing configuration.
- Verify repeated taps, slider drags, navigation, and scrolling do not cause resets, reloads, resource restarts, or visible flicker.

## Delivery phases

1. **Design foundation:** blue-accent tokens, typography, spacing, surfaces, control hierarchy, and component states.
2. **Navigation:** align Android bottom navigation and Windows rail to Record, Editor, Creator, and Library.
3. **Existing workflows:** refine Record, Editor, Processing/A-B, and Library without changing audio semantics.
4. **Creator foundation:** introduce project persistence and focused Creator navigation.
5. **Creator workflows:** scripts, teleprompter, transcripts/captions, and production markers.
6. **Advanced workflow:** phrase replacement after verified audio alignment and non-destructive assembly exist.
7. **Polish:** accessibility, responsive behavior, performance, and platform validation.

## Safety and validation gates

- No audio-engine or DSP semantic changes in visual-only slices.
- Preserve recording, save/recovery, playback, waveform, trim, processing, cancellation, and no-overwrite behavior.
- All configured CI jobs must pass.
- Manual checks are required for actual device behavior, visual usability, interaction smoothness, and microphone behavior.
