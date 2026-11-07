# Phase G — Android Settings parity & open-items closeout

**Status:** Contract on tip  
**Context:** Hybrid Studio UI phases **A–F** are merged. Phase A deferred a full **Android Settings destination** because mid-file injects into `MainActivity.kt` caused `compileDebugKotlin` failures. Windows already has an additive Settings entry/panel path where CI apply succeeds.

## Goal

1. Ship a **safe** Android Settings surface that matches product sections (Recording / Processing / Appearance / Models / About) without hiding Record, Editor, Creator, or Library.
2. Track remaining **non-UI** open items so they are not lost after the hybrid phase table closed.

## Android Settings — required behavior

| Rule | Detail |
|------|--------|
| Additive only | Settings is a destination or overlay; primary nav always returns to Record / Editor / Creator / Library |
| Sections | Recording, Processing, Appearance, Models and offline, About and copyright |
| Processing | Prefer existing preference contract / Natural reset; do not remove Editor processing controls |
| Theme | System vs Dark studio (Phase E) |
| Compile safety | Prefer a **dedicated** Kotlin file or explicit destination enum branch — **not** string-search inject into the middle of MainActivity builders |

### Recommended implementation path

1. Add `SettingsScreen.kt` (or similar) with section UI in one place.
2. Extend the existing bottom-nav / destination switch with a `Settings` case that shows that content.
3. Wire Processing reset to the same preference helpers Editor already uses.
4. Keep TalkBack names on all interactive controls (`docs/A11Y_FULL_PASS.md`).

### Explicitly out of scope for a risky CI inject

- Do not insert `Button` / `contentDescription` fragments by searching for `"Library"` or `listOf(...)` in MainActivity.
- Do not `GONE` other workspaces permanently when opening Settings.

## Open items (product / release)

| Item | Notes |
|------|--------|
| Manual smoke | Record + teleprompter couple; trim/process non-destructive; Creator save/reload |
| TalkBack / Narrator | Checklist in `docs/A11Y_FULL_PASS.md` |
| Listening fixtures | Optional QA assets for VAD / denoise perception |
| Android Settings code | This phase — dedicated file + nav branch |
| Windows Settings polish | Already additive; keep parity with Android sections |

## Acceptance

- [ ] Docs describe Android Settings parity and open items.
- [ ] CI remains green (no broken MainActivity inject).
- [ ] When Settings UI lands on Android, all four primary destinations stay reachable.
