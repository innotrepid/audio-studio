# Local Transcription and Production Audio Roadmap

Status: architecture decision and implementation plan. This document does not claim Whisper.cpp, Silero VAD, the resampler, alignment, splicing, or punch-in recording are implemented.

## Model decision

Use the **77 MiB Whisper.cpp model variant** selected by the project owner as the initial transcription candidate. Keep model selection configurable so the project can compare accuracy, memory use, latency, and language coverage against other quantized variants without changing application code.

Before pinning the exact artifact, record its upstream filename, release/revision, license, SHA-256, and verified byte size. Do not infer exact quantization or language coverage from displayed size alone.

- Runtime: whisper.cpp, integrated through the shared/native audio architecture where practical.
- Input: 16,000 Hz mono PCM, with tested resampling and explicit sample-format/range handling.
- Output: transcript segments and word-level start/end timestamps, with confidence/quality metadata where available.
- Intended use: local narration transcription, word timing, script alignment, retake selection, and caption drafting. Timings are anchors to refine—not guaranteed edit boundaries.
- Product requirement: Synvidra must work offline immediately after installation, without a post-install model download or separate model setup.

## Model distribution and repository policy

**Bundle the required inference models in distributable APK and Windows release packages.** A user must not need to download models after installing Synvidra.

1. Do not commit large model binaries into ordinary Git history. Store pinned model artifacts in a controlled upstream release or GitHub Release asset, and have CI retrieve them during packaging. If a model must be supplied manually, provide its official source link and checksum instructions to the project owner for upload to the repository's release assets—not as a normal source commit.
2. Add explicit ignore rules for temporary model/cache/download directories and model binary extensions, while checking that no tracked source assets are accidentally hidden.
3. Pin exact upstream artifact URLs/revisions, license notices, SHA-256 hashes, and verified byte sizes in a text manifest.
4. CI must fetch each pinned artifact, verify size and checksum, and fail packaging on mismatch or unavailable assets.
5. Package models into each supported release artifact and verify their presence in the APK and Windows package. The app must resolve bundled model paths reliably and must not depend on network access at runtime.
6. Keep model version and status visible in diagnostics; support safe model replacement/upgrade in a future app update without damaging projects.
7. Include required model license/attribution notices in the app distribution and documentation. Confirm redistribution terms before bundling any model.
8. Keep package-size, install-time, storage-footprint, and runtime-memory budgets explicit; do not silently omit a model to meet a size limit.

## Silero VAD

Evaluate and integrate Silero Voice Activity Detection for speech/silence regions and pause candidates. Confirm the exact upstream artifact, license, version, checksum, supported runtime, and input contract before distribution. If selected as a required feature, bundle its model in the APK and Windows package under the same verified-artifact policy. VAD regions may propose chunk boundaries, cut points, and room-tone sampling windows; they must not automatically destructively edit source audio.

## Production audio pipeline

### 1. Audio preparation
- Decode supported source audio without modifying it.
- Resample to 16 kHz mono for Whisper input.
- Preserve original sample rate, channel count, frame count, and source URI/path metadata.
- Test resampler output length, channel downmix, clipping, and edge cases.

### 2. Transcription and word timing
- Run local Whisper.cpp inference.
- Store transcript text, segment timing, per-word start/end time, model identity, and processing status.
- Validate monotonic timing, bounds against source duration, empty/low-confidence output, and repeated words.

### 3. Script-to-spoken alignment
- Align recognized words against script words using sequence-aware matching rather than positional word-index assumptions.
- Handle flubs, repeated phrases, skipped lines, and ad-libs.
- Preserve unmatched spoken and script words; surface ambiguity rather than silently forcing a match.
- Give anchors stable IDs and retain the script revision used for alignment.

### 4. Non-destructive project model
- Keep source recordings immutable.
- Store word-level timing and stable word/phrase anchors.
- Store edits as an ordered, validated edit list referencing source ranges and replacement takes.
- Track source identity, script revision, timing basis, and stale/orphan anchor status.
- Persist and round-trip the model through save, recovery, export, and import.

### 5. VAD-assisted cut planning
- Detect speech regions and pauses.
- Propose cut points with configurable safety margins.
- Sample room tone from suitable source regions and record its source range.
- Require preview/confirmation for ambiguous cuts; avoid clipping phonemes or breaths.

### 6. Splicer and derived render
- Render edits to a **new derived file**, never overwrite the original.
- Apply short, configurable crossfades where appropriate.
- Match replacement loudness to surrounding narration using measured loudness.
- Fill gaps with sampled room tone where appropriate.
- Keep render settings and edit provenance in the project; support preview and undo by editing the list.

### 7. Punch-in recording
- Select a word/phrase range, play a configurable lead-in, and record a replacement take.
- Preserve original and replacement takes.
- Use measured input/output latency compensation and explicit calibration metadata.
- Align takes to verified audio frame/time anchors, not word indices alone.

## Remaining feature-completion gate before audio engine work

Finish and test the existing Creator/Production foundations first:
- Word/phrase-anchored production markers with stable section/word IDs.
- Replacement-request model: section/script revision, selected word range, exact phrase, surrounding context, pending/reviewed state, and stable ID.
- Stale-anchor detection after script edits, section deletion/reordering, or transcript changes.
- Orphan handling that never silently reassigns a marker or replacement request.
- Android and Windows persistence, import/export, and recovery round-trip coverage.
- Production workspace usability and regression tests.
- Windows draft-store backup recovery review.
- Confirm project schema migration behavior before introducing word-level data.

## Acceptance test matrix

Use deterministic audio fixtures covering clean narration, silence, room tone, pauses, repeated words, flubs, ad-libs, clipped starts/ends, and replacement takes.
- Bundled model: verify model presence in fresh APK and Windows package; install and run transcription offline with network disabled.
- Packaging: verify model checksums, license notices, package contents, and documented size budgets.
- Resampler: mono/stereo sources, varied sample rates, duration preservation, no unexpected clipping.
- Transcription: known reference transcript, word timestamps within defined tolerance, empty/no-speech handling.
- Alignment: skips, insertions, repetitions, ad-libs, and ambiguous matches.
- VAD: pause detection and boundary tolerance against annotated fixtures.
- Edit list: save/reload/import/export, undo, stale anchors, and source immutability.
- Splicer: no clicks at joins, no unintended gaps, loudness consistency, room-tone continuity, and derived-file provenance.
- Punch-in: lead-in behavior, measured latency, repeatability, and alignment tolerance on real Android hardware.
- Platform parity: Android and Windows project behavior and data compatibility.

## Implementation sequence

A. Finish Production markers/replacement-request foundations and recovery tests.
B. Establish pinned model manifest, redistribution/license review, CI artifact retrieval, APK/Windows bundling, and diagnostics.
C. Add resampler and Whisper.cpp integration with word-timing tests.
D. Add Silero VAD and annotated pause tests.
E. Add script-to-spoken alignment and word-level project schema.
F. Add cut planning and room-tone sampling.
G. Add non-destructive splicer and derived rendering.
H. Add punch-in recording and measured latency compensation.

Do not describe phases B–H as complete until implementation and acceptance tests pass on relevant CI jobs and target devices.
