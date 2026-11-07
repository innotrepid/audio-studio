# Phase 7 — Cross-platform polish and release readiness

**Status:** In progress  
**Branch:** `Redesign,-Teleprompter,-captions-&-more`  
**Depends on:** Phase 5–6 complete on tip (including soft assembly crossfade)

## Done upstream (Phases 5–6)

- Caption timing provenance, take approval, transcript–take link
- Phrase replacement: select → manual range → link take → A/B → assembly preview → approve / discard
- Optional `derivedPath` on approved assemblies; source takes never overwritten
- Soft linear crossfade (~15 ms) at assembly splice joins

## Phase 7 goals

1. **Accessibility** — content descriptions, keyboard focus order (Windows), font scaling
2. **Project backup/restore** — export/import project JSON + referenced take paths policy
3. **Manual validation** — Android device + Windows desktop smoke of Creator → Record → phrase path
4. **Docs** — README, changelog, architecture, testing strategy, roadmap match shipped behavior
5. **Release packaging** — green CI; models still verified in APK / Windows publish output

## Slices

| Slice | Outcome | Status |
|-------|---------|--------|
| 7.1 Soft crossfade on phrase assembly | ~15 ms linear joins | Done |
| 7.2 Docs sync | ROADMAP + redesign status + this file | Done |
| 7.3 Accessibility pass | Creator Captions / Record teleprompter labels | **This change** |
| 7.4 Project backup/restore | Harden import/export + tests | Next |
| 7.5 Changelog + README Creator section | After 7.3–7.4 | Open |

## Notes for 7.3

- Android: `exchangeButton` already sets `contentDescription`; studio teleprompter Start/Pause and scroll-speed SeekBar get explicit descriptions.
- Windows: after XAML restore/injects, `AutomationProperties.Name` is applied to Creator and phrase-related buttons that lacked names.

## Exit gate (from CREATOR_STUDIO_REDESIGN)

Android and Windows workflows usable end to end; CI green; manual test evidence recorded; known limitations documented.
