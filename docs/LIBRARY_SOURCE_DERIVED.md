# Library source vs derived (Hybrid Studio Phase D)

**Status:** Contract on tip  
**Principle:** Redesign clarifies file roles; it **never removes** Library list, open, or search. Search filters loaded entries only — it must **not** trigger a storage rescan.

## Goal

Users always know whether an item is **source** (original capture) or **derived** (trim, process, phrase assembly, export), matching the product rule: **never silent overwrite of source audio**.

## Categories

| Badge | Meaning | Typical paths |
|-------|---------|----------------|
| **Source** | Original recording or imported original | Finalized capture WAV, user import |
| **Derived** | Produced by edit/process/assembly | Trim export, processed WAV, phrase assembly, caption-side media |
| **Project** | Creator project metadata (not audio) | Creator JSON / backup |

Platform folder rules stay as implemented; this phase does **not** invent delete or rename APIs.

## Hierarchy (Library workspace)

| Layer | Content |
|-------|---------|
| 1. Search / filter | Filter loaded list only |
| 2. List rows | Name + **Source/Derived** badge + duration if known |
| 3. Actions | Open in Editor, play if available — existing only |

## Non-goals

- No new delete/rename/move pipelines.
- No change to recovery or capture finalization.
- No Android mid-file UI injects (compile-safe).

## Acceptance

- [ ] Docs and product language use Source / Derived consistently.
- [ ] Existing Library actions remain available.
- [ ] CI green.
