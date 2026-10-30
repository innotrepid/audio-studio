# Product Requirements

## Requirement language

**Must** means required for the stated milestone. **Should** means strongly preferred. **Later** means not required for the recording prototype.

## Phase 1 — Recording foundation

### Capture
- Must request and handle microphone permission correctly.
- Must record audio from a supported input device and save it to a user-accessible project/file location.
- Must show recording state, elapsed time, input level, and a clear clipping warning where measurable.
- Must allow recording to stop safely and finalize the file.
- Must report failures such as permission denial, unavailable input, low storage, or write errors without pretending the recording succeeded.
- Must preserve the original captured audio.
- Should support pause/resume only when the selected platform implementation can do so safely.
- Must avoid blocking audio capture on UI, network, database, or long-running processing tasks.

### Playback and review
- Must allow a completed recording to be played back.
- Must show basic duration and file details.
- Must make incomplete or failed recordings distinguishable from finalized files.

### Quality baseline
- Must document sample rate, channel count, encoding, and processing state for saved audio.
- Must detect digital clipping where the captured format permits it.
- Must establish a repeatable baseline test protocol for noise, latency, and dropouts.
- No noise-removal or dereverberation quality claim is accepted until validated with representative recordings and listening tests.

## Phase 2 — Intelligent editor
- Waveform or equivalent time-based navigation.
- Multiple takes linked to script sections.
- Non-destructive trimming and assembly.
- Transcript-based selection and editing when transcription is available.
- Silence and filler-word detection must produce reviewable suggestions, not silently delete speech.
- Replacement takes must preserve the rest of the approved narration.
- Undo/redo and recovery from interrupted editing.

## Phase 3 — Creator studio
- Script storage and version history.
- Teleprompter with configurable text size and scroll behavior.
- Script sections linked to recordings.
- Narration duration and timestamps based on actual audio.
- Editable captions with SRT and VTT export.
- User-editable production markers and project templates.
- Export of narration and supporting metadata.

## Phase 4 — Optional AI integrations
- Provider adapters must be isolated from core recording.
- API credentials must not be hardcoded or committed.
- Users must understand when audio/text leaves their device.
- Provider errors, usage limits, and unavailable network access must be handled clearly.
- Generated audio must be distinguishable from user-recorded audio.
- Voice generation must follow provider terms and consent requirements.

## Phase 5 — Advanced intelligence
- Compare recording sessions for level, noise, spectral balance, and reverberation differences.
- Suggest corrective actions and let users approve them.
- Offer enhanced restoration only when model licensing, quality, latency, and hardware requirements are acceptable.
- Generate narration-aligned production blueprints as suggestions, not verified facts about footage.

## Cross-platform requirements
- Android and Windows are first-class targets.
- Shared project concepts and core business rules should be platform-independent.
- Permissions, input selection, audio focus/interruption handling, device routing, and file access must use appropriate platform behavior.
- Small-screen recording UX and desktop editing UX may differ.
- Project files should be versioned and recoverable.

## Privacy and reliability
- Recording is initiated and stopped explicitly by the user.
- No hidden recording.
- No upload without clear user action and disclosure.
- No destructive processing of the only copy of a recording.
- No credentials, private recordings, or private scripts in logs or repository fixtures.
- Autosave and crash recovery should be introduced before complex editing workflows.

## Release gate

A phase is complete only when its acceptance criteria, relevant tests, known limitations, and platform results are documented. A green build is necessary but does not prove audio quality.


## Hybrid Studio experience requirements

- The primary navigation exposes Record, Editor, Creator, and Library as the four main destinations.
- Settings remains secondary; processing and A/B comparison are contextual Editor workflows.
- Android uses a touch-first layout and Windows a desktop-first layout while retaining shared product concepts and terminology.
- The primary interactive accent is blue. Mint is reserved for live/healthy audio states; warning and error colors have clear semantic meaning.
- Visual-only changes preserve recording, recovery, playback, waveform, trim, processing, cancellation, and source-file safety behavior.

## Creator project foundation requirements (planned)

- Project, script, section, cue, take, transcript segment, caption cue, production marker, and replacement-request records use stable identifiers.
- Persisted project data is versioned from its first release and supports migration and recovery.
- Autosave and safe atomic persistence are required before complex Creator editing is considered complete.
- Original audio remains immutable; edits and assembled results are separate derived assets.
- Transcript text without verified timing must never be presented as audio-synchronized.
- Phrase replacement requires a verified audio time/frame range; word indices alone are insufficient.
