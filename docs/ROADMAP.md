# Roadmap

## Current baseline

Audio Studio is a native Android and Windows application with a shared C++ audio core. The initial native foundation, recording workflows, WAV handling, waveform analysis, non-destructive trim, and offline processing capabilities have progressed beyond the early roadmap phases. Check the platform READMEs and CI for the exact implementation and validation status of individual capabilities.

## Phase 0 — Hybrid Studio design foundation

- [x] Native Android/Kotlin/NDK/Oboe and Windows/WinUI application architecture established.
- [x] Shared C++ core and Linux/Windows core CI established.
- [x] Android and Windows application build workflows established in CI.
- [x] Record, waveform, trim, library, and processing workflows implemented to varying levels of platform validation.
- [ ] Establish shared blue-accent visual tokens, typography, spacing, surfaces, and component states.
- [ ] Align navigation to Record, Editor, Creator, and Library while preserving current screen instances and state.
- [ ] Reconcile documentation and testing guidance with current native architecture and shipped behavior.

**Exit criteria:** current docs agree on architecture and capability status; design tokens are documented; visual foundation changes preserve existing audio behavior; configured CI jobs pass.

## Phase 1 — Existing workflow refinement

- Refine Record hierarchy, input monitoring, recording state, clipping warnings, and post-save actions.
- Add an integrated Studio Teleprompter to the Record workflow: select a Creator script, show it alongside recording controls, run a configurable pre-roll countdown, then scroll in sync with the active take. Include manual speed, pause/resume, typography controls, section navigation, and saved script-position recovery. Treat camera/video preview as a separate platform capability; voice-follow scrolling is a later advanced enhancement.
- Refine Editor waveform scale, selection feedback, timecodes, preview, trim, processing presets, and A/B review.
- Refine Library clarity and source/derived asset distinctions without unexpectedly broadening storage discovery.
- Preserve incremental UI updates and loaded state across navigation.

**Exit criteria:** existing record/save/recovery/playback/trim/process/cancel behavior remains intact; Android and Windows CI pass; focused manual smoke checks are recorded.

## Phase 2 — Creator and project foundation

- Add the Creator destination and focused sub-navigation.
- Define versioned project/script/take metadata with stable IDs.
- Implement atomic persistence, autosave, recovery, and migration tests.
- Associate takes with projects without moving or rewriting source audio.

**Exit criteria:** project data survives restart; malformed imports fail safely; recovery and migration behavior are tested; standalone recordings remain accessible.

## Phase 3 — Scripts and teleprompter

- Script editing and organization into stable, reorderable sections.
- Section notes and production cues.
- Dedicated teleprompter reading mode with adjustable typography, line spacing, scroll speed, and section navigation.
- Persist teleprompter preferences and restore them reliably.
- Link recording sessions to the active project/script section using the existing native recording engine.

**Exit criteria:** edit, autosave, reload, scroll, manual navigation, and section-linked recording tests pass on supported platforms.

## Phase 4 — Transcripts, captions, and production

- Editable transcript segments associated with source takes.
- Caption cue editing with explicit timing and timing provenance.
- SRT and WebVTT export with timestamp/ordering/encoding tests.
- Production markers, retake notes, and take approval states.
- Evaluate transcription/forced alignment separately; never represent unverified timing as synchronized.

**Exit criteria:** captions round-trip through validated exports; markers persist and recover; user review remains in control.

## Phase 5 — Phrase replacement and audio assembly

- Select transcript text and resolve it to a verified audio time/frame range.
- Record replacement takes through the existing native recording path.
- Audition original and replacement takes.
- Create non-destructive assembly previews with tested boundary/crossfade handling.
- Preserve all source and replacement recordings; approval creates a new derived asset.

**Exit criteria:** uncertain alignment requires manual correction; rejected replacements leave source audio unchanged; technical tests and structured listening checks pass.

## Phase 6 — Cross-platform polish and release readiness

- Accessibility, keyboard support, font scaling, responsive layouts, and performance profiling.
- Project backup/restore and migration validation.
- Physical Android and Windows runtime testing.
- Update release notes, limitations, architecture, and test evidence.
- Verify all configured CI jobs.

**Exit criteria:** end-to-end workflows are usable on both platforms, known limitations are documented, and CI plus manual platform checks pass.

## Engineering constraints

- Work only on the agreed repository branch; do not create a parallel audio engine or replace the native architecture with WebView/Flutter.
- Preserve Oboe/native recording, C++ processing, WAV finalization, recovery, waveform analysis, trim, and cancellation behavior.
- Visual-only work must not change recording or DSP semantics.
- Never overwrite original recordings during trim, processing, or phrase replacement.
- Keep file I/O and heavy processing off the UI thread and real-time audio callback.
- Update only affected controls; avoid rebuilding entire screens for routine interactions.
- CI success is not proof of microphone behavior or perceptual audio quality.
