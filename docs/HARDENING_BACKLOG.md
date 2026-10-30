# Engineering hardening backlog

This is the durable parking lot for reliability issues that are worth revisiting but should not block product progress indefinitely.

## Working rule: hardening has a budget

- Fix data-loss, source-corruption, security, or crash-level defects immediately when confirmed.
- Otherwise, record the issue here with impact and a revisit trigger instead of expanding the current hardening pass.
- Keep each reliability PR narrowly scoped and testable.
- Once the current in-flight hardening PR is green and merged, return to user-facing capability work. Do not start another general hardening sweep unless a listed trigger occurs.
- CI green is required for merging, but does not replace real-device audio and Windows launch validation.

## Open / deferred items

| ID | Item | Risk / impact | Revisit trigger | Status |
|---|---|---|---|---|
| H-01 | Atomic no-replace publication of processed WAVs | A narrow race may allow a destination created between the existence check and final rename to be overwritten on platforms where rename replaces an existing file. | Before shipping processing as production-ready, or if destination-collision testing exposes it. | Implemented in PR #80 with atomic same-directory hard-link publication and late-destination regression coverage; confirm all CI targets. |
| H-02 | Broader WAV reader size limits and hostile-file resource bounds | The standalone reader loads the entire WAV into memory; very large or malformed files may create avoidable memory pressure. The streaming processor is separately bounded. | Before expanding the reader to arbitrary external files or adding batch processing. | Deferred; define a configurable limit or streaming reader design first. |
| H-03 | Broader parser fuzzing / malformed RIFF corpus | Existing regression tests cover known malformed cases, but not broad fuzzing or a large fixture corpus. | Before release candidate or after a parser crash/corruption report. | Deferred; add fuzz target/corpus as a dedicated task. |
| H-04 | Physical-device audio quality matrix | Gate quality depends on voice, room noise, and settings; CI cannot establish listening quality. | Before claiming advanced noise cancellation or release-quality enhancement. | Pending user hardware time; collect quiet speech gaps, steady noise, keyboard/fan noise, and reverberant-room samples. |
| H-05 | Windows runtime launch and capture validation | Windows CI compilation/publish has passed, but the user previously encountered a XAML parse failure on their machine. | When the user's Windows installation/toolchain is ready; don't block independent feature development. | Paused by user; resume from startup diagnostics and packaged artifact. |
| H-06 | Extended platform stress tests: device changes, interruptions, low storage, abrupt termination | Important platform-specific failure modes have not all been exercised on physical devices. | Before a release candidate or when hardware testing becomes available. | Deferred; maintain explicit test checklist. |

## Recently completed hardening (for traceability)

- [x] WAV recovery validation and failure-path tests.
- [x] Batched PCM16 writer output and recording-duration integrity test.
- [x] Reject NaN and infinite samples without advancing frame count.
- [x] Reject frame-count overflow before reading samples.
- [x] Reject truncated processing input without publishing output or leaving orphan partials.
- [x] Cancellation at initial progress callback cleans temporary output.
- [x] Processing cleanup does not delete an unrelated destination file created concurrently.
- [x] Progress callback exceptions become descriptive errors and clean writer-owned temporary output (PR #53; merged after all three CI jobs passed).

## Current product-work priority

The recording-confidence, waveform-navigation, trim, and processing milestones have progressed substantially since this backlog was first written. The current product priority is the Hybrid Studio redesign and Creator workflow foundation, not another broad reliability sweep.

1. Establish shared visual tokens and align Android/Windows navigation without changing audio behavior.
2. Refine existing Record, Editor, and Library experiences while preserving their current behavior.
3. Add Creator and project persistence only with versioning, autosave, recovery, and migration tests.
4. Introduce scripts, teleprompter, transcripts/captions, and production markers in separately testable slices.

Keep confirmed data-loss, corruption, security, and crash defects as immediate fixes. For every product feature, add the safety checks needed for that feature and return unrelated concerns to this backlog.
