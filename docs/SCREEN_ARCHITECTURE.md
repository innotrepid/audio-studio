# Audio Studio — Screen Architecture

## Goal

Provide one connected voice-production workflow without overwhelming creators or compromising precise audio tools. Preserve the native audio engine and file-safety behavior while giving each task a clear home.

## Navigation model

The shared product destinations are:

1. **Record** — capture speech and monitor input.
2. **Editor** — waveform, trim, processing, playback comparison, and export.
3. **Creator** — scripts, teleprompter, transcripts/captions, and production.
4. **Library** — projects, recordings, takes, and derived outputs as project capabilities arrive.

Settings remains a secondary app-level destination. Processing and Compare are contextual Editor workflows, not separate top-level destinations.

### Android

- Four-item bottom navigation: Record, Editor, Creator, Library.
- Touch-first, vertically composed screens with comfortable targets and readable timecodes.
- Keep recording controls and live input feedback immediately visible.
- Creator uses focused sub-navigation rather than one long screen.
- Preserve the existing activity and screen state where practical; navigation must not restart audio resources or discard loaded selections/preferences.

### Windows

- Persistent left navigation rail: Record, Editor, Creator, Library.
- Wider work area for waveform editing, contextual controls, and side-by-side comparison.
- Keyboard navigation, visible focus, scalable window layouts, and accessible labels.
- Same concepts and terminology as Android, with desktop-appropriate layouts.

## Record

**Purpose:** Start and stop a recording confidently.

- Input-device selector and refresh.
- One prominent Start/Stop action with clear text and state.
- Elapsed time, live RMS/peak meter, and clipping warning.
- Clear permission, device, storage, and finalization errors.
- Interrupted-recording recovery remains discoverable but secondary.
- After saving, offer playback, opening in Editor, and future project/section association.
- Do not put waveform trim or advanced processing controls here.

Preserve native capture, bounded queue/writer, WAV finalization, recovery, device selection, and existing meter/clipping behavior.

## Editor

**Purpose:** Inspect and refine audio safely.

- Waveform is the dominant visual element.
- Clear selection handles/shading and precise start/end/duration readouts.
- Preview selection and export/trim remain separate actions.
- Processing presets are presented before advanced controls.
- Advanced parameters are collapsed by default.
- Original and derived files are clearly distinguished.
- Progress, cancellation, and errors do not erase the current file or selection.
- Processing and trim always create derived output; never silently overwrite the source.

Preserve existing waveform analysis, cached peaks, frame-accurate trim, selected-range preview, processing, cancellation, and source-file integrity.

## Creator

Creator is a collection of focused workspaces.

### Scripts
- Create and organize scripts within projects.
- Divide scripts into stable, reorderable sections.
- Support section notes and production cues.
- Autosave with visible save state and safe recovery.

### Teleprompter
- Dedicated distraction-reduced reading mode.
- Adjustable font size, line spacing, text width, scroll speed, and start position.
- Clear start, pause, resume, reset, and section navigation.
- Manual scrolling coexists predictably with automatic scrolling.
- Persist preferences and avoid rebuilding the whole screen on every scroll tick.

### Transcripts & Captions
- Editable transcript segments associated with a source take.
- Caption cues have explicit start/end times and editable text.
- Show timing provenance (manual, imported, or generated/aligned).
- Do not present untimed text as synchronized.
- Validate SRT and WebVTT exports.
- Transcription/forced alignment requires a separate quality and reliability gate.

### Production
- Markers can reference a project, script section, take, or verified audio range.
- Support retake notes, pronunciation checks, review flags, and approval states.
- Suggestions never silently modify source recordings or approved narration.

## Library

- Find recordings and generated versions using current platform storage permissions/discovery boundaries.
- Distinguish source recordings, alternate takes, processed copies, trimmed exports, and final outputs.
- Search should filter loaded results without unexpectedly rescanning storage.
- Destructive actions require explicit confirmation and must not be confused with opening/exporting.
- Project grouping arrives with the versioned project foundation; do not imply it exists before implementation.

## Visual direction

- Near-black/charcoal foundation with opaque layered surfaces.
- Blue is the primary interactive accent for selected navigation, primary actions, and active editing tools.
- Mint is reserved for live/healthy audio states; warm colors communicate warnings and errors.
- Strong typographic hierarchy, consistent spacing, readable timecodes, subtle borders, and clear action hierarchy.
- Avoid pervasive blur/glass, excessive card borders, and decorative visualizers.
- Color is never the only signal; pair it with text, icons, and state.

## Performance and accessibility

- Keep screen instances alive across navigation where practical.
- Update only the affected control for routine interactions.
- Do not re-read/re-analyze WAV files during waveform selection drags.
- Keep file I/O and processing off the UI thread.
- Respect Android touch targets/font scaling and Windows keyboard/focus/accessibility conventions.
- Design loading, empty, permission, error, success, and disabled states.

## Implementation sequence

1. Design tokens and navigation foundation.
2. Refine Record, Editor, and Library without changing audio behavior.
3. Add Creator and versioned project persistence.
4. Add scripts and teleprompter.
5. Add transcripts, captions, and production markers.
6. Add phrase replacement only after verified transcript-to-audio alignment and non-destructive assembly are available.
7. Cross-platform polish and release validation.

## Acceptance criteria

- Existing recording, save/recovery, playback, processing, waveform, preview, trim, cancellation, and no-overwrite behavior remains intact.
- Live RMS/peak monitoring and clipping warnings remain visible during recording.
- Users can identify the primary action in each workspace without being confronted by every advanced control.
- Original recordings are never overwritten by processing, trimming, or assembly.
- Android and Windows share concepts but use appropriate layouts.
- All configured CI jobs pass, followed by physical-device/desktop manual checks.
