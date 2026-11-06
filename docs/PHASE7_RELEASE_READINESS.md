# Phase 7 — Cross-platform polish and release readiness

**Status:** Started  
**Branch:** `Redesign,-Teleprompter,-captions-&-more`  
**Depends on:** Phase 5 (captions/production) and Phase 6 (phrase replacement) complete on tip

## Done upstream (Phases 5–6)

- Caption timing provenance, take approval, transcript–take link
- Phrase replacement: select → manual range → link take → A/B → assembly preview → approve / discard
- Optional `derivedPath` on approved assemblies; source takes never overwritten
- Soft linear crossfade (~15 ms) at assembly splice joins (this PR)

## Phase 7 goals

1. **Accessibility** — content descriptions, keyboard focus order (Windows), font scaling
2. **Project backup/restore** — export/import project JSON + referenced take paths policy
3. **Manual validation** — Android device + Windows desktop smoke of Creator → Record → phrase path
4. **Docs** — README, changelog, architecture, testing strategy, roadmap match shipped behavior
5. **Release packaging** — green CI; models still verified in APK / Windows publish output

## First slices (ordered)

| Slice | Outcome |
|-------|---------|
| 7.1 Soft crossfade on phrase assembly | Done in this change |
| 7.2 Docs sync (this file + ROADMAP + redesign status) | This change |
| 7.3 Accessibility pass on Creator Captions / Record teleprompter controls | Next |
| 7.4 Project backup/restore contract + tests | Next |
| 7.5 Changelog + README Creator section | After 7.3–7.4 |

## Exit gate (from CREATOR_STUDIO_REDESIGN)

Android and Windows workflows usable end to end; CI green; manual test evidence recorded; known limitations documented.
