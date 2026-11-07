# Phase G — Android Settings parity & open-items closeout

**Status:** Real UI in progress (`SettingsScreen.kt` + nav wiring)  
**Context:** Hybrid Studio UI phases **A–F** are merged. Phase A deferred a full **Android Settings destination** because mid-file injects into `MainActivity.kt` caused `compileDebugKotlin` failures.

## Implementation (this branch)

| Piece | Role |
|-------|------|
| `SettingsScreen.kt` | Dedicated UI: Recording, Processing (Natural reset), Appearance (theme prefs), Models, About |
| `ci_apply_android_settings_real_ui.py` | Surgical anchors only: section decl, GONE init, `root.addView`, nav visibility, bottom nav item |
| Non-hiding | Settings is a fifth **secondary** nav target; Record/Editor/Creator/Library remain |

Processing reset calls existing `applyProcessingPreset("Natural")`.

## Open items

| Item | Notes |
|------|--------|
| Manual smoke | Settings opens; bottom nav returns to Record |
| Theme apply | Preference stored; full chrome application may still be incremental |
| TalkBack | Controls named in SettingsScreen |

## Acceptance

- [ ] Settings appears in Android bottom nav
- [ ] Primary destinations still reachable
- [ ] CI green (compileDebugKotlin)
