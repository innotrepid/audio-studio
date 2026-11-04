# UI roadmap

Audio Studio uses a **Hybrid Studio** experience: approachable Record and Creator workflows alongside precise Editor and Processing workspaces. Keep the native audio engine and file-safety behavior stable while improving navigation, hierarchy, and feedback.

## Experience principles

- One primary action per workspace.
- Source audio is never overwritten by trim, processing, or replacement.
- Status text is specific and actionable.
- Prefer updating existing controls over full-screen rebuilds.
- Platform-native layouts; shared product language and blue accent tokens.

## Primary navigation

1. Record
2. Editor
3. Creator
4. Library

Secondary surfaces (Processing, recovery, settings) attach to these roots rather than competing as peer tabs.

## Workspace intent

### Record

- Capture a clean take with clear input, level, and clipping feedback.
- **Studio teleprompter** is dominant and **page chrome is not scrollable**.
- Teleprompter text and settings come from **Creator → Teleprompter**, not from Scripts widgets or Record-local settings (`docs/CREATOR-TELEPROMPTER-FLOW.md`).
- Record start starts teleprompter scroll; pause pauses both.

### Editor

- Waveform-first selection, timecodes, non-destructive trim export.

### Processing

- Presets first; advanced controls secondary; always A/B source vs processed.

### Creator

- Scripts: section-based editor, notes, cues, autosave. **Source of script text.**
- Teleprompter: auto-mirrors active script; **owns** pace/typography/spacing/countdown; feeds Record studio teleprompter (`docs/CREATOR-TELEPROMPTER-FLOW.md`).
- Transcripts & Captions: editable segments, explicit timing provenance, SRT/WebVTT export.
- Production: markers, retake notes, review and approval states.

### Library

- Clear source/derived file categories and safe next actions.
- Search filters loaded entries; it must not trigger a storage rescan.
- Preserve platform-specific folder rules and never invent delete/rename in this phase.

## Delivery order

1. Visual foundation and shared tokens.
2. **Navigation:** align Android bottom navigation and Windows rail to Record, Editor, Creator, and Library.
3. **Record studio teleprompter layout + coupled transport** (this branch).
4. **Creator foundation:** project persistence and focused Creator navigation.
5. **Creator workflows:** scripts, teleprompter workspace, transcripts/captions, and production markers.
6. Accessibility and release polish.

## Validation

CI must pass for Android, Linux core, and Windows core/shell. Manual checks cover microphone capture, teleprompter scroll/pause coupling, non-destructive trim/process, and Creator save/reload. Windows runtime validation depends on the maintainer’s Windows environment.
