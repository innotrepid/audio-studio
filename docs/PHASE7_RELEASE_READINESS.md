# Phase 7 — Cross-platform polish and release readiness

**Status:** In progress  
**Branch:** `Redesign,-Teleprompter,-captions-&-more`

## Done upstream (Phases 5–6)

- Caption timing provenance, take approval, transcript–take link
- Phrase replacement through approve/discard + soft crossfade
- `derivedPath` on approved assemblies; source takes never overwritten

## Slices

| Slice | Outcome | Status |
|-------|---------|--------|
| 7.1 Soft crossfade | ~15 ms linear joins | Done |
| 7.2 Docs sync | ROADMAP + redesign status | Done |
| 7.3 Accessibility | Teleprompter + Captions labels | Done |
| 7.4 Project backup/restore | Import/export JSON + tests | **This change** |
| 7.5 Changelog + README | Creator section | Next |

## 7.4 contract

- **Export** serializes the validated project document only (schema v1).
- **Takes** remain path/URI references; audio bytes are **not** packaged.
- **Import** runs the same validation as `save` / `LoadOrCreate`; on failure the **active project is not replaced**.
- Successful import is written through the existing atomic save path.

## Exit gate

Android and Windows workflows usable end to end; CI green; manual test evidence recorded; known limitations documented.
