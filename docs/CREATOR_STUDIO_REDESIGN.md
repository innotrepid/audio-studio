# Audio Studio — Hybrid Studio Redesign Blueprint

**Status:** Phases 0–6 largely implemented on tip; **Phase 7 started** — see `docs/PHASE7_RELEASE_READINESS.md` and `docs/ROADMAP.md`  
**Target repository:** `innotrepid/audio-studio`  
**Target branch:** `Redesign,-Teleprompter,-captions-&-more`  
**Scope:** Android and Windows native applications  
**Design direction:** Hybrid studio

### Implementation status (2026-11)

| Phase | Topic | Status |
|-------|--------|--------|
| 0–1 | Design tokens, navigation shell | Done |
| 2 | Record/Editor/Library visual refinement | Partial |
| 3 | Creator persistence | Done |
| 4 | Script + teleprompter | Done |
| 5 | Captions / production | Done |
| 6 | Phrase replacement + assembly (+ soft crossfade, approve/discard) | Done (listening fixtures open) |
| 7 | Polish and release readiness | **In progress** |

---

## 1. Product experience

Audio Studio should feel like one connected voice-production workspace:

**Plan → Script → Record → Review → Refine → Publish**

The application has two complementary levels:

- **Creator experience:** approachable and task-oriented. It helps a creator prepare a script, read it, record sections, review transcript/caption content, and track production.
- **Audio engineering experience:** precise and powerful. It exposes waveform editing, trim, processing, playback comparison, and technical feedback without burdening the everyday creator workflow.

The visual direction is a restrained, polished dark studio: near-black and charcoal foundations, opaque layered surfaces, blue as the primary interactive accent, and mint for live/healthy audio states. Warm colors are reserved for warnings and destructive actions. Avoid pervasive glass/blur, excessive borders, decorative visualizers, and card-wrapping every control.

## 2–9

The full screen specifications, delivery phases (0–7), and engineering constraints remain in git history of this path and in the Phase sections below as previously published on this branch. Prefer `docs/ROADMAP.md` and `docs/PHASE7_RELEASE_READINESS.md` for live status while Phase 7 doc sync consolidates the long-form blueprint.

### Phase 6 (complete on tip)

- Verified transcript-to-audio range selection (manual)
- Replacement takes via existing capture engine
- A/B audition and non-destructive assembly preview
- Approve → derived WAV + `derivedPath`; discard → `assemblyStatus=discarded`
- Soft linear crossfade (~15 ms) at splice joins
- Source takes never overwritten

### Phase 7 (in progress)

- Accessibility, keyboard, font scaling
- Project backup/restore
- Manual device/desktop validation
- Docs/README/changelog alignment
- Clean CI and release packaging

## Non-negotiable engineering constraints

- Work only in `innotrepid/audio-studio` on `Redesign,-Teleprompter,-captions-&-more`.
- Do not create another repository or branch for this work outside the agreed tip branch workflow.
- Do not create a second audio engine or replace the current native architecture with a WebView/cross-platform framework.
- Preserve Oboe/native recording, C++ processing, WAV finalization, recovery, waveform analysis, trim, and cancellation behavior.
- Never overwrite source recordings during processing, trimming, or phrase replacement.
- Keep expensive file I/O and audio processing off the UI thread.
- CI success is not proof of visual usability or audio quality; physical-device and desktop manual checks remain required.
