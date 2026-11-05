# Roadmap

## Current baseline

Audio Studio is a native Android and Windows application with a shared C++ audio core. The initial native foundation, recording workflows, WAV handling, waveform analysis, non-destructive trim, and offline processing capabilities have progressed beyond the early roadmap phases. Check the platform READMEs and CI for the exact implementation and validation status of individual capabilities.

## Phase 0 — Hybrid Studio design foundation

- [x] Native Android/Kotlin/NDK/Oboe and Windows/WinUI application architecture established.
- [x] Shared C++ core and Linux/Windows core CI established.
- [x] Android and Windows application build workflows established in CI.
- [x] Record, waveform, trim, library, and processing workflows implemented to varying levels of platform validation.
- [x] Establish shared blue-accent visual tokens, typography, spacing, surfaces, and component states.
- [x] Align navigation to Record, Editor, Creator, and Library while preserving current screen instances and state.
- [x] Reconcile documentation and testing guidance with current native architecture and shipped behavior.

**Exit criteria:** current docs agree on architecture and capability status; design tokens are documented; visual foundation changes preserve existing audio behavior; configured CI jobs pass.

## Phase 1 — Existing workflow refinement

- Refine Record hierarchy, input monitoring, recording state, clipping warnings, and post-save actions.
- [x] Deliver the first audio-only Studio Teleprompter slice on Android and Windows (script-in-Record, countdown, scroll after record start). Android hands-on confirmed; CI green.
- [x] Add adjustable teleprompter text size and line spacing controls (initially on Record; **moving ownership to Creator → Teleprompter** per `docs/CREATOR-TELEPROMPTER-FLOW.md`).
- [ ] **Record layout:** non-scrollable Record workspace; maximize studio teleprompter; keep only essential controls (record/pause, clipping/level, compact status). Teleprompter **settings live under Creator → Teleprompter**, not Record.
- [ ] **Single data path:** Scripts → auto-mirror into Creator Teleprompter → Record studio teleprompter uses Teleprompter text+settings (not Scripts widgets).
- [ ] **Coupled transport:** record start starts teleprompter scroll; pause pauses recording and scroll together.
- [ ] Add section navigation and section-aware script editing/recording; persist and restore teleprompter reading position. Camera/video preview remains separate; voice-follow scrolling remains later.
- Refine Editor waveform scale, selection feedback, timecodes, preview, trim, processing presets, and A/B review.
- Refine Library clarity and source/derived asset presentation without changing scan or file-safety rules.

**Exit criteria:** primary workflows remain usable after visual refinement; no recording or processing regressions; CI remains green.

## Phase 2 — Creator and project foundation

- [x] Define the cross-platform versioned project/script/take JSON contract, stable IDs, validation, and safe-write requirements in `docs/PROJECT-SCRIPT-DATA-CONTRACT.md`.
- [x] Add focused Creator sub-navigation and a functional Scripts workspace.
- [ ] Implement atomic persistence, autosave, recovery, and migration tests on Android and Windows.
- [x] Associate verified takes with projects without moving or rewriting source audio.

**Implementation note:** Creator exposes Scripts, Teleprompter, Captions, and Production on both platforms. Scripts and project persistence are functional. **Teleprompter is becoming a real workspace** (script mirror + settings ownership for the Record studio teleprompter)—see `docs/CREATOR-TELEPROMPTER-FLOW.md`. Captions/Production remain thinner until Phase 4 exit criteria.

**Exit criteria:** project data survives restart; malformed imports fail safely; recovery and migration behavior are tested; standalone recordings remain accessible.

## Phase 3 — Scripts and teleprompter

- Script editing and organization into stable, reorderable sections.
- Section notes and production cues.
- **Creator → Teleprompter workspace:** auto-shows active script text; owns speed, text size, line spacing, countdown defaults; persists preferences.
- **Record studio teleprompter:** large, non-scrollable page chrome; consumes Teleprompter text+settings; record/pause coupled to scroll (see `docs/CREATOR-TELEPROMPTER-FLOW.md`).
- Section navigation while reading/recording; persist and restore reading position.
- Link recording sessions to the active project/script section using the existing native recording engine.

**Exit criteria:** edit, autosave, reload, Teleprompter mirror, Record layout, coupled transport, and section-linked recording checks pass on supported platforms.

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
