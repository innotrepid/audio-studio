# Hybrid Studio UI — Implementation Plan

**Status:** Phase A (Settings plan + NOTICE) merged; **Phase B Record hierarchy in progress**  
**Principle:** Redesign is **app-wide** and **non-destructive** — do not hide screens, remove buttons, or disable functions. Navigation may focus a destination; every primary destination remains reachable.

## 1. Modern hybrid studio patterns

| Pattern | Synvidra mapping |
|---------|------------------|
| Few primary destinations | Record · Editor · Creator · Library |
| Secondary settings | Settings entry — not a replacement for primary tabs |
| Hero surface per mode | Teleprompter on Record; waveform on Editor |
| Presets before pro | Processing presets → advanced |
| Source vs derived | Never silent overwrite |
| Dark studio chrome | Charcoal + blue + mint live |
| Attribution | `NOTICE.md` (Innotrepid + third parties) |

## 2. Non-hiding rule

- UI redesign **adds or restyles**; it does not permanently hide workspaces.
- Settings must not strand the user away from Record/Editor/Creator/Library.
- CI apply scripts must not strip existing controls.

## 3. Settings (Phase A)

- Plan + `NOTICE.md` on tip.
- Windows: additive Settings entry/panel where applied.
- Android: full Settings chrome deferred until a safe destination-switch anchor exists (no mid-expression injects).

## 4. Phases

| Phase | Work | Status |
|-------|------|--------|
| A | Settings plan + notices; additive entry where safe | Done (Android UI deferred) |
| B | Record hierarchy polish | **This change** — see `docs/RECORD_HIERARCHY.md` |
| C | Editor preset-first chrome | Next |
| D | Library source/derived badges | Open |
| E | Theme applied app-wide | Open |
| F | Full a11y pass | Open |

## 5. Licensing

See repository root **`NOTICE.md`**: Synvidra © Innotrepid (all rights reserved by default); third-party MIT/Apache/BSD components listed with upstream links.
