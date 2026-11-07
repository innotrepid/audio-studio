# Hybrid Studio UI — Implementation Plan

**Status:** Phases **A–F merged**; **Phase G** Android Settings parity and open-items closeout  
**Principle:** Redesign is **app-wide** and **non-destructive** — do not hide screens, remove buttons, or disable functions.

## 1. Patterns

| Pattern | Synvidra mapping |
|---------|------------------|
| Few primary destinations | Record · Editor · Creator · Library |
| Secondary settings | Settings entry — not a peer tab |
| Hero surface | Teleprompter (Record); waveform (Editor) |
| Presets before pro | Processing presets → advanced |
| Source vs derived | Library badges |
| Dark studio chrome | `docs/THEME_APP_WIDE.md` |
| Accessibility | `docs/A11Y_FULL_PASS.md` |
| Attribution | `NOTICE.md` |

## 2. Non-hiding rule

UI redesign adds or restyles; CI apply scripts must not strip existing controls. Android Settings must use a dedicated screen/module, not mid-builder string injects.

## 3. Phases

| Phase | Work | Status |
|-------|------|--------|
| A | Settings plan + NOTICE | Done (Android Settings chrome deferred) |
| B | Record hierarchy | Done — `docs/RECORD_HIERARCHY.md` |
| C | Editor preset-first | Done — `docs/EDITOR_PRESET_FIRST.md` |
| D | Library source/derived | Done — `docs/LIBRARY_SOURCE_DERIVED.md` |
| E | Theme app-wide | Done — `docs/THEME_APP_WIDE.md` |
| F | Full a11y pass | Done — `docs/A11Y_FULL_PASS.md` |
| G | Android Settings parity + open-items closeout | **This change** — `docs/PHASE_G_ANDROID_SETTINGS.md` |

## 4. Licensing

See **`NOTICE.md`**: Synvidra © Innotrepid; third-party MIT/Apache/BSD components listed.
