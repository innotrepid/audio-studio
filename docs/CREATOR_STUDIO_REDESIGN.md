# Audio Studio — Hybrid Studio Redesign Blueprint

**Status:** Design and implementation blueprint  
**Target repository:** `innotrepid/audio-studio`  
**Target branch:** `Redesign,-Teleprompter,-captions-&-more`  
**Scope:** Android and Windows native applications  
**Design direction:** Hybrid studio

This document defines the product experience and delivery sequence for the next Audio Studio redesign. It is deliberately a blueprint, not a claim that the redesign has already been implemented.

## 1. Product experience

Audio Studio should feel like one connected voice-production workspace:

**Plan → Script → Record → Review → Refine → Publish**

The application has two complementary levels:

- **Creator experience:** approachable and task-oriented. It helps a creator prepare a script, read it, record sections, review transcript/caption content, and track production.
- **Audio engineering experience:** precise and powerful. It exposes waveform editing, trim, processing, playback comparison, and technical feedback without burdening the everyday creator workflow.

The visual direction is a restrained, polished dark studio: near-black and charcoal foundations, opaque layered surfaces, violet as the primary interactive accent, and mint for live/healthy audio states. Warm colors are reserved for warnings and destructive actions. Avoid pervasive glass/blur, excessive borders, decorative visualizers, and card-wrapping every control.

## 2. Information architecture

### Primary destinations

| Destination | Purpose | Key actions |
|---|---|---|
| Record | Capture speech confidently | Select input, monitor signal, record/stop, recover, review saved take |
| Editor | Refine audio safely | Open take, inspect waveform, select range, preview, trim, process, compare |
| Creator | Prepare and manage spoken content | Scripts, teleprompter, transcripts/captions, production markers |
| Library | Find project assets and outputs | Browse projects, takes, processed copies, trims, exports |

Settings remains a secondary app-level destination for infrequent configuration. Processing and A/B comparison are contextual Editor tools rather than separate top-level destinations. This reduces navigation competition while retaining the capabilities already present.

### Android

- Four-item bottom navigation: Record, Editor, Creator, Library.
- Touch-first vertical layouts with comfortable targets and readable timecodes.
- Keep recording controls and live input feedback immediately visible.
- Use focused sub-navigation inside Creator; do not put all Creator controls on one long page.
- Preserve the current activity and existing screen instances where practical. Navigation must not restart audio resources or discard selections and preferences.

### Windows

- Persistent left navigation rail: Record, Editor, Creator, Library.
- Wider work area with contextual tool panels and side-by-side comparison where useful.
- Support keyboard navigation, shortcuts for common actions, scalable window sizes, and visible focus states.
- Keep the same product concepts and terminology as Android while using desktop-appropriate layouts.

## 3. Screen and workspace specifications

### Record

**Primary goal:** start and stop a recording confidently.

- Prominent single Start/Stop recording action with clear text and state.
- Elapsed time, input selector, live RMS/peak feedback, and clipping warning.
- Clear permission, unavailable-device, storage, and write/finalization errors.
- Interrupted-recording recovery is discoverable but secondary.
- After save, show the take and clear next actions: listen, open in Editor, or associate it with a project/script section.
- Do not place waveform trim controls or advanced noise-processing parameters on this screen.

**Preserve:** Oboe/native capture, bounded queue and writer, WAV finalization, recovery, device selection, meter and clipping behavior.

### Editor

**Primary goal:** make precise, reversible audio changes.

- Waveform is the dominant visual element.
- Selection handles and selected-region shading are clear at phone and desktop sizes.
- Start, end, and selected duration are readable and precise.
- Preview selection and export/trim are separate actions.
- Processing starts from clear presets; advanced parameters remain collapsed until requested.
- Original and derived files are clearly distinguished.
- Progress, cancellation, and errors do not erase the current selection or loaded file.
- Original audio is never silently overwritten.

**Preserve:** existing waveform analysis, cached peaks, frame-accurate trim selection, selected-range preview, non-destructive trim export, streaming processing, cancellation, RNNoise/DeepFilterNet3 status and limitations, and existing native audio APIs. Neural denoising is currently offline processing; do not present it as a live recording filter.

### Creator

Creator is a set of focused workspaces, not one vertically endless screen.

#### Scripts

- Create, rename, duplicate, and organize scripts within a project.
- Divide scripts into stable, reorderable sections.
- Support section notes and production cues such as emphasis, pause, pronunciation, and visual/B-roll reminders.
- Autosave with visible save state and safe recovery.
- Keep editing comfortable on phone and desktop.

#### Teleprompter

- Present one active section in a distraction-reduced reading view.
- Adjustable font size, line spacing, text width, scroll speed, and start position.
- Start, pause, resume, and reset controls must be obvious and reachable.
- Section navigation and progress must remain visible without obscuring the script.
- Manual scrolling must coexist predictably with automatic scrolling.
- Persist user preferences and restore them reliably.
- Avoid scroll implementations that rebuild or reflow the whole screen on every tick.

#### Transcripts & Captions

- Transcript segments are editable and associated with a source take.
- Caption cues have explicit start/end times and editable text.
- Timing provenance must be visible: manually timed, imported, or generated/aligned.
- Never imply that untimed text is synchronized to audio.
- Export SRT and WebVTT with validated timestamps, ordering, encoding, and line formatting.
- Transcription or forced alignment can be added only when a dependable implementation is available; user correction remains central.

#### Production

- Production markers attach to a project, section, take, or verified audio time range.
- Marker examples: retake needed, pronunciation check, remove false start, add pause, review noise, approved.
- Retake notes and take status remain visible in context.
- Users explicitly approve edits and selected takes; suggestions never silently change the original recording.

### Library

- Organize assets by project when project support is introduced.
- Distinguish source recordings, alternate takes, processed copies, trimmed exports, and final exports.
- Keep current platform discovery boundaries and permissions clear during transition.
- Search should filter loaded results without unexpectedly rescanning storage.
- Destructive actions require confirmation and must never be confused with opening or exporting.

## 4. Visual system

### Color roles

- **Base:** deep charcoal / near-black.
- **Raised surfaces:** opaque charcoal layers with restrained borders.
- **Primary accent:** violet for selected navigation, primary actions, and active editing tools.
- **Live/healthy accent:** mint for recording state, healthy signal, and successful completion.
- **Warning:** warm amber for clipping and recoverable issues.
- **Error/destructive:** red, used sparingly and always paired with explanatory text.

Color is never the only way to communicate state. Use labels, icons, shape, and status text together.

### Typography and spacing

- Strong page title, concise section labels, readable supporting text.
- Use tabular numerals for timers and timecodes where supported.
- Establish a consistent spacing scale, control height, corner radius, and focus treatment.
- Prefer fewer, better-grouped surfaces over a stack of generic cards.
- Keep important actions visually dominant and secondary actions quiet.

### Interaction and accessibility

- Every icon-only action has an accessible name and tooltip/description where appropriate.
- Touch targets are comfortable on Android.
- Windows supports keyboard navigation and visible focus.
- Respect system font scaling and narrow window sizes.
- Loading, empty, permission, error, success, and disabled states are designed—not left to default widgets.

## 5. Shared project and data model

Introduce project organization incrementally. Do not block the visual redesign on a large database migration.

The initial model should be versioned and migration-friendly. Candidate entities:

- **Project:** stable ID, title, created/updated timestamps, schema version.
- **Script:** stable ID, project ID, title, ordered sections.
- **ScriptSection:** stable ID, script ID, text, order, notes, cues.
- **Cue:** stable ID, section ID, cue type, text, optional range/reference.
- **Take:** stable ID, project ID, source file reference, optional section ID, duration, status, creation time.
- **ProductionMarker:** stable ID, project ID, optional take/section reference, marker type, note, optional verified frame range.
- **TranscriptSegment:** stable ID, take ID, text, timing, timing provenance, review state.
- **CaptionCue:** stable ID, take ID, text, start/end time, export metadata.
- **ReplacementRequest:** stable ID, take ID, text anchor, verified audio-frame/time anchor when available, replacement-take references, review state.

Rules:

1. IDs are stable; display order is not identity.
2. Audio files remain immutable source assets. Edits create derived assets or explicit assembly outputs.
3. Persist references and metadata separately from audio bytes.
4. Autosave and crash recovery are required before project workflows are considered reliable.
5. Include schema version and migrations from the first persisted project format.
6. Do not use a word index as an audio edit position. Phrase replacement requires a verified time/frame anchor.
7. Start with a versioned project document and atomic writes if that meets platform requirements. Evaluate SQLite only when querying, concurrency, or scale justifies it.

## 6. Phrase replacement: future workflow and safety

Phrase replacement is a high-value capability, but it is not a simple text operation.

Proposed flow:

1. User selects a word or phrase in a reviewed transcript.
2. App resolves the selection to a verified audio time/frame range. If alignment is unavailable or uncertain, user must place/adjust the range manually.
3. User records one or more replacement takes using the existing native recording engine.
4. App presents original phrase and replacement for audition.
5. App creates a non-destructive assembly preview with controlled boundaries and crossfade where technically appropriate.
6. User approves or rejects the result.
7. Approved output is a new derived asset; original take and replacement takes remain available.

Do not promise seamless blending until listening tests cover timing, room tone, loudness, pitch, consonant boundaries, and crossfade artifacts. Extend the existing shared C++ audio core for assembly only when required; do not create a second audio engine.

## 7. Implementation plan

Deliver in small, reviewable commits on the target branch. Each slice must preserve the current working app and pass relevant CI before proceeding.

### Phase 0 — Baseline and design tokens

- Confirm branch HEAD and green Android, Linux C++ core, and Windows CI.
- Inventory existing colors, typography, navigation, and reusable controls on both platforms.
- Define token names and values for color roles, spacing, typography, radii, elevation, and control states.
- Capture current behavior as a regression checklist.
- No audio-engine or DSP changes.

**Exit gate:** design tokens documented; current record/edit/process/library flows remain unchanged and baseline CI is green.

### Phase 1 — Navigation shell and visual foundation

- Android: introduce the four primary destinations while preserving current native views and their state.
- Windows: align the navigation rail and workspace labels with the same information architecture.
- Introduce reusable page headers, action hierarchy, status treatments, and surface styles.
- Keep Settings secondary and processing/comparison reachable from Editor.
- Do not add empty placeholder screens that imply unfinished features are complete.

**Exit gate:** navigation works on both platforms, state survives switching, accessible labels exist, and all three CI jobs pass.

### Phase 2 — Record, Editor, and Library visual refinement

- Refine Record hierarchy and post-save actions.
- Refine waveform scale, trim feedback, timecodes, preview, and safe export.
- Refine preset-first processing and original/processed comparison within Editor.
- Refine Library grouping and source/derived distinctions without changing discovery boundaries.
- Profile scrolling, waveform gestures, and incremental UI updates.

**Exit gate:** existing recording, recovery, playback, trim, processing, cancellation, and no-overwrite tests pass; manual smoke checks confirm no state loss or visible interaction regressions.

### Phase 3 — Creator foundation and project persistence

- Add the Creator destination and focused sub-navigation.
- Define and test versioned project/script data structures.
- Implement atomic persistence, autosave, recovery, and migration tests.
- Associate existing takes with projects without moving or rewriting their audio data.

**Exit gate:** project data survives restart, malformed imports fail safely, recovery is tested, and existing standalone recordings remain accessible.

### Phase 4 — Script editor and teleprompter

- Implement script and section editing.
- Add cues and section navigation.
- Build a dedicated teleprompter reading mode and settings.
- Persist pace, font size, line spacing, and scroll preferences.
- Link a recording session to the active project/script section without changing native capture semantics.

**Exit gate:** editing, autosave, reload, scrolling, manual navigation, and section-linked recording pass Android and Windows tests.

### Phase 5 — Transcript, captions, and production markers

- Add editable transcript segments and timing provenance.
- Add caption cue editing and SRT/WebVTT export.
- Add production markers and retake notes.
- Add import/export validation and round-trip tests.
- Keep transcription/forced alignment behind a separately evaluated capability gate.

**Exit gate:** timestamps are valid and ordered, exports are independently checked, and no unverified alignment is presented as fact.

### Phase 6 — Phrase replacement and assembly

- Implement verified transcript-to-audio range selection.
- Record replacement takes with the existing capture engine.
- Add audition and non-destructive assembly preview.
- Extend the shared C++ core only for the required assembly/crossfade operations.
- Add listening fixtures and boundary/artifact tests.

**Exit gate:** original audio is preserved, replacement can be rejected safely, and approved output passes both technical checks and structured listening evaluation.

### Phase 7 — Cross-platform polish and release readiness

- Accessibility, keyboard support, font scaling, responsive layouts, and performance profiling.
- Project backup/restore and migration validation.
- Android physical-device testing and Windows runtime testing.
- Update README, changelog, architecture, testing strategy, and roadmap to match implemented behavior.
- Verify clean CI and release packaging.

**Exit gate:** Android and Windows workflows are usable end to end; all CI jobs pass; manual test evidence is recorded; known limitations are documented.

## 8. Non-negotiable engineering constraints

- Work only in `innotrepid/audio-studio` on `Redesign,-Teleprompter,-captions-&-more`.
- Do not create another repository or branch for this work.
- Do not create a second audio engine or replace the current native architecture with a WebView/cross-platform framework.
- Preserve Oboe/native recording, C++ processing, WAV finalization, recovery, waveform analysis, trim, and cancellation behavior.
- No visual-only slice may change recording or DSP semantics.
- Never overwrite source recordings during processing, trimming, or phrase replacement.
- Keep expensive file I/O and audio processing off the UI thread.
- Update only the affected UI controls; do not rebuild entire screens for routine state changes.
- Keep screen instances and loaded state alive across navigation where practical.
- CI success is not proof of visual usability or audio quality; physical-device and desktop manual checks remain required.
- Do not claim a feature is complete until its target platform builds, normal and failure paths are tested, and user-facing limitations are documented.

## 9. First implementation slice

After this blueprint is reviewed, the first code slice should be deliberately small:

1. Add shared design tokens and reusable visual styles.
2. Align navigation labels and selected states with Record, Editor, Creator, and Library.
3. Keep the existing screens and native audio behavior intact.
4. Add the Creator entry as a clearly marked foundation only when it has a useful initial screen; do not ship misleading dead-end controls.
5. Run the repository's Android, Linux C++ core, and Windows CI jobs.

This gives the project a stable visual foundation before adding the larger Creator workflows.
