# Hybrid Studio UI — Implementation Plan

**Status:** Phases A–D merged; **Phase E theme app-wide in progress**  
**Principle:** Redesign is **app-wide** and **non-destructive** — do not hide screens, remove buttons, or disable functions.

## 1. Patterns

| Pattern | Synvidra mapping |
|---------|------------------|
| Few primary destinations | Record · Editor · Creator · Library |
| Secondary settings | Settings entry — not a peer tab |
| Hero surface | Teleprompter (Record); waveform (Editor) |
| Presets before pro | Processing presets → advanced |
| Source vs derived | Library badges — `docs/LIBRARY_SOURCE_DERIVED.md` |
| Dark studio chrome | Phase E — `docs/THEME_APP_WIDE.md` |
| Attribution | `NOTICE.md` |

## 2. Non-hiding rule

UI redesign adds or restyles; CI apply scripts must not strip existing controls.

## 3. Phases

| Phase | Work | Status |
|-------|------|--------|
| A | Settings plan + NOTICE | Done |
| B | Record hierarchy | Done — `docs/RECORD_HIERARCHY.md` |
| C | Editor preset-first | Done — `docs/EDITOR_PRESET_FIRST.md` |
| D | Library source/derived | Done — `docs/LIBRARY_SOURCE_DERIVED.md` |
| E | Theme app-wide | **This change** — `docs/THEME_APP_WIDE.md` |
| F | Full a11y pass | Next |

## 4. Licensing

See **`NOTICE.md`**: Synvidra © Innotrepid; third-party MIT/Apache/BSD components listed.
