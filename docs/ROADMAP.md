# Roadmap

The roadmap is ordered by risk. Recording reliability and audio quality come before advanced UI and AI features.

## Phase 0 — Repository and technical validation
- [x] Establish product direction and documentation.
- [ ] Scaffold Flutter project for Android and Windows.
- [ ] Establish CI for formatting, analysis, tests, and supported builds.
- [ ] Evaluate capture APIs and candidate audio libraries/models, including licenses and platform support.
- [ ] Record initial architecture decisions.

**Exit criteria:** reproducible development setup and a green baseline CI run.

## Phase 1 — Recording foundation
- [ ] Microphone permission and device availability handling.
- [ ] Reliable capture and file finalization.
- [ ] Recording state, timer, input level, and clipping warnings.
- [ ] Playback and basic file details.
- [ ] Preserve source recordings and handle failures safely.
- [ ] Baseline audio-quality and performance tests on Android and Windows.
- [ ] Compare raw audio against any candidate processing chain.

**Exit criteria:** users can record and replay speech reliably on both platforms, with no known data-loss defect in the tested workflow. Processing quality must be documented honestly.

## Phase 2 — Intelligent editor
- [ ] Non-destructive trimming and assembly.
- [ ] Takes and targeted retakes.
- [ ] Waveform navigation.
- [ ] Speech transcription investigation.
- [ ] Reviewable silence and filler-word suggestions.
- [ ] Undo/redo and recovery.

**Exit criteria:** a test narration with intentional mistakes can be repaired without re-recording unaffected sections.

## Phase 3 — Creator studio
- [ ] Script editor and project organization.
- [ ] Teleprompter and section-based recording.
- [ ] Script-to-audio alignment and timing.
- [ ] Editable captions and SRT/VTT export.
- [ ] Production markers, templates, and batch export.

**Exit criteria:** a script can move through recording and assembly into an organized narration package.

## Phase 4 — Optional AI integrations
- [ ] Define provider adapter interface.
- [ ] Add supported voice-generation integration only after checking current API terms and cost.
- [ ] Import, compare, and mix human and synthetic narration.
- [ ] Optional transcription/script assistance.
- [ ] Clearly disclose cloud processing and data transfer.

**Exit criteria:** core app works without configured providers; integration failures do not break local projects.

## Phase 5 — Advanced intelligence
- [ ] Session consistency analysis.
- [ ] Quality-control report and recommended corrections.
- [ ] Evaluate local speech-enhancement and dereverberation models.
- [ ] Production blueprint and narration-synchronized markers.
- [ ] Optional authorized personal voice model research.

**Exit criteria:** each intelligent feature has demonstrated usefulness, clear limits, and repeatable validation.

## Working rule

Advance one bounded milestone at a time. Keep the main branch buildable, avoid broad rewrites, and do not claim a phase is complete until its exit criteria are met.
