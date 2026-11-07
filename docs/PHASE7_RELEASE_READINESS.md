# Phase 7 — Cross-platform polish and release readiness

**Status:** In progress (7.1–7.5 docs slice)  
**Branch:** `Redesign,-Teleprompter,-captions-&-more`

## Slices

| Slice | Outcome | Status |
|-------|---------|--------|
| 7.1 Soft crossfade | ~15 ms linear joins | Done |
| 7.2 Docs sync | ROADMAP + redesign status | Done |
| 7.3 Accessibility | Teleprompter + Captions labels | Done |
| 7.4 Project backup/restore | Import/export JSON + tests | Done |
| 7.5 Changelog + README Creator section | Align product docs with tip | **This change** |
| 7.6 Manual smoke + release packaging notes | Device/desktop checklist | Open |

## 7.4 contract (shipped)

- Export serializes the validated project document only (schema v1).
- Takes remain path/URI references; audio bytes are not packaged.
- Import runs the same validation as save; on failure the active project is not replaced.
- Successful import uses the existing atomic save path.

## Exit gate

Android and Windows workflows usable end to end; CI green; manual test evidence recorded; known limitations documented.
