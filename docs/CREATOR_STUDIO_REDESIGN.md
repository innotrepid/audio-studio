# Audio Studio — Hybrid Studio Redesign Blueprint

**Status:** Phases 0–6 largely implemented on tip; **Phase 7 started** (see `docs/PHASE7_RELEASE_READINESS.md`)  
**Target repository:** `innotrepid/audio-studio`  
**Target branch:** `Redesign,-Teleprompter,-captions-&-more`  
**Scope:** Android and Windows native applications  
**Design direction:** Hybrid studio

This document defines the product experience and delivery sequence for the Hybrid Studio redesign. Implementation status is tracked in `docs/ROADMAP.md` and `docs/PHASE7_RELEASE_READINESS.md`. Historical sections below remain the design contract.

## 1. Product experience

Audio Studio should feel like one connected voice-production workspace:

**Plan → Script → Record → Review → Refine → Publish**

The application has two complementary levels:

- **Creator experience:** approachable and task-oriented. It helps a creator prepare a script, read it, record sections, review transcript/caption content, and track production.
- **Audio engineering experience:** precise and powerful. It exposes waveform editing, trim, processing, playback comparison, and technical feedback without burdening the everyday creator workflow.

The visual direction is a restrained, polished dark studio: near-black and charcoal foundations, opaque layered surfaces, blue as the primary interactive accent, and mint for live/healthy audio states. Warm colors are reserved for warnings and destructive actions. Avoid pervasive glass/blur, excessive borders, decorative visualizers, and card-wrapping every control.

## Implementation status (2026-11)

| Phase | Topic | Status |
|-------|--------|--------|
| 0–1 | Design tokens, navigation shell | Done |
| 2 | Record/Editor/Library visual refinement | Partial |
| 3 | Creator persistence | Done |
| 4 | Script + teleprompter | Done |
| 5 | Captions / production | Done |
| 6 | Phrase replacement + assembly (+ soft crossfade, approve/discard) | Done (listening fixtures open) |
| 7 | Polish and release readiness | **In progress** |

For the full original blueprint (screens, constraints, exit gates), retain git history of this file prior to the Phase 7 kickoff edit, or expand sections below as needed during Phase 7 doc sync.

## Non-negotiable engineering constraints (unchanged)

- Work only in `innotrepid/audio-studio` on `Redesign,-Teleprompter,-captions-&-more`.
- Preserve Oboe/native recording, C++ processing, WAV finalization, recovery, waveform analysis, trim, and cancellation behavior.
- Never overwrite source recordings during processing, trimming, or phrase replacement.
- CI success is not proof of visual usability or audio quality; physical-device and desktop manual checks remain required.
