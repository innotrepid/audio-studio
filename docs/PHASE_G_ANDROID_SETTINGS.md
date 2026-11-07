# Phase G — Android Settings parity & open-items closeout

**Status:** Contract on tip  
**Context:** Hybrid Studio UI phases **A–F** are merged. Phase A deferred a full **Android Settings destination** because mid-file injects into `MainActivity.kt` caused `compileDebugKotlin` failures. Windows already has an additive Settings entry/panel path where CI apply succeeds.

## Goal

1. Ship a **safe** Android Settings surface that matches the product sections (Recording / Processing / Appearance / Models / About) without hiding Record, Editor, Creator, or Library.
2. Track remaining **non-UI** open items so they are not lost after the hybrid phase table closed.

## Android Settings — required behavior

| Rule | Detail |
|------|--------|
| Additive only | Settings is a destination or overlay; primary nav always returns to Record/Editor/Creator/Library |
| Sections | Same as Windows / `NOTICE.md` About: Recording, Processing, Appearance, Models & offline, About & copyright |
| Processing | Prefer existing preference contract / Natural reset — do not remove Editor processing controls |
| Theme | System vs Dark studio (Phase E) |
| Compile safety | Prefer a **dedicated** Kotlin file or explicit destination enum branch over string-search inject into the middle of `MainActivity` builders |

### Recommended implementation path (no CI string-inject)

1. Add `SettingsScreen.kt` (or similar) with the section UI built in one place.
2. Extend the existing bottom-nav / destination switch with a `