# Hybrid Studio UI — Implementation Plan

**Status:** Phases A–C merged; **Phase D Library source/derived in progress**  
**Principle:** Redesign is **app-wide** and **non-destructive** — do not hide screens, remove buttons, or disable functions.

## 1. Patterns

| Pattern | Synvidra mapping |
|---------|------------------|
| Few primary destinations | Record · Editor · Creator · Library |
| Secondary settings | Settings entry — not a peer tab |
| Hero surface | Teleprompter (Record); waveform (Editor) |
| Presets before pro | Processing presets → advanced |
| Source vs derived | Never silent overwrite — Library badges |
| Attribution | `NOTICE.md` |

## 2. Non-hiding rule

UI redesign adds or restyles; CI apply scripts must not strip existing controls.

## 3. Phases

| Phase | Work | Status |
|-------|------|--------|
| A | Settings plan + NOTICE | Done (Android Settings chrome deferred) |
| B | Record hierarchy | Done — `docs/RECORD_HIERARCHY.md` |
| C | Editor preset-first | Done — `docs/EDITOR_PRESET_FIRST.md` |
| D | Library source/derived badges | **This change** — `docs/LIBRARY_SOURCE_DERIVED.md` |
| E | Theme app-wide | Next |
| F | Full a11y pass | Open |

## 4. Licensing

See **`NOTICE.md`**: Synvidra © Innotrepid; third-party MIT/Apache/BSD components listed.
