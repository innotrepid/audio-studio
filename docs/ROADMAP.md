# Roadmap

## Current baseline

Audio Studio is a native Android and Windows application with a shared C++ audio core. The initial native foundation, recording workflows, WAV handling, waveform analysis, non-destructive trim, and offline processing capabilities have progressed beyond the early roadmap phases. Check the platform READMEs and CI for the exact implementation and validation status of individual capabilities.

**Creator Studio (2026-11 tip):** Phases 5–6 of the redesign blueprint are implemented on `Redesign,-Teleprompter,-captions-&-more` (captions production depth, phrase replacement through approve/discard, soft assembly crossfade). **Phase 7 (release readiness)** is in progress — see `docs/PHASE7_RELEASE_READINESS.md`.

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
- Refine Editor waveform, selection, preview, and non-destructive trim feedback.
- Refine processing entry points and original vs derived presentation.
- Refine Library grouping without changing discovery boundaries.

## Phase 2–4 — Creator foundation, script, teleprompter

- [x] Creator destination and project/script persistence.
- [x] Script sections, teleprompter settings, reading position, section navigation.
- [x] Record-screen studio teleprompter bound to Creator teleprompter data.

## Phase 5 — Captions and production markers

- [x] Draft captions with timing provenance.
- [x] Take approval states and transcript–take link.

## Phase 6 — Phrase replacement and assembly

- [x] Phrase request model, manual range, replacement take link, A/B audition.
- [x] Non-destructive assembly preview and approve (derived WAV + `derivedPath`).
- [x] Discard assembly (`assemblyStatus=discarded`).
- [x] Soft linear crossfade (~15 ms) at splice joins.
- [ ] Structured listening fixtures / boundary artifact tests (still open).

## Phase 7 — Cross-platform polish and release readiness

- [ ] Accessibility and keyboard pass on Creator / Record teleprompter.
- [ ] Project backup/restore contract and tests.
- [ ] README / changelog / testing strategy aligned with shipped Creator flows.
- [ ] Manual Android device + Windows desktop smoke evidence.
- [ ] Green CI and release packaging verification.

See `docs/PHASE7_RELEASE_READINESS.md` and `docs/CREATOR_STUDIO_REDESIGN.md`.
