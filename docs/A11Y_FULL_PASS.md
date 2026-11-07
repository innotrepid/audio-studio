# Full accessibility pass (Hybrid Studio Phase F)

**Status:** Contract on tip  
**Principle:** Accessibility **adds** names, roles, and focus order; it **never removes** screens, buttons, or functions.

## Goal

Every primary control in Record, Editor, Creator, Library, and Settings is reachable and named for assistive tech on Android (`contentDescription` / TalkBack) and Windows (`AutomationProperties.Name` / Narrator).

## Already shipped (build on this)

- Phase 7 accessibility apply (`ci_apply_phase7_accessibility.py`): teleprompter and related controls
- Incremental Names on Record transport, processing presets, Library, theme radios (Phases B–E where anchors existed)

## Checklist by destination

| Destination | Must name |
|-------------|-----------|
| **Record** | Record / Pause / Stop; teleprompter surface; level/clip status; device refresh; recovery entry |
| **Editor** | Play; selection; export/trim; A/B; processing presets; advanced sliders |
| **Creator** | Scripts / Teleprompter / Captions / Production subnav; save/export; section nav |
| **Library** | Search field; list rows; open actions; Source/Derived language in row text when shown |
| **Settings** | Section headers; reset processing; theme radios; back via primary nav |

## Focus & non-goals

- Prefer platform defaults for focus order; do not trap focus in Settings.
- No custom screen-reader-only redesign that hides visible controls.
- No Android mid-expression `contentDescription` injects into `MainActivity.kt` (compile-safe).

## Manual validation

- [ ] TalkBack: navigate Record transport and Creator subnav.
- [ ] Narrator: Windows rail + Settings Appearance radios.
- [ ] CI green (automated labels only where safe).

## Relation to release

Pairs with `PHASE7_RELEASE_READINESS.md` accessibility slice; this document is the product-level full-pass checklist for Hybrid Studio UI phases A–F.
