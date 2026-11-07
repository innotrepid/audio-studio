# Hybrid Studio UI — Implementation Plan

**Status:** Planning + Settings surface (additive)  
**Principle:** Redesign is **app-wide** and **non-destructive to UX surface area** — do not hide screens, remove buttons, or disable functions as part of visual redesign. Navigation may *focus* a destination; every primary destination remains reachable.

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

- UI redesign **adds or restyles**; it does not `GONE`-away entire workspaces permanently.
- Settings must not leave the user unable to return to Record/Editor/Creator/Library.
- CI apply scripts must not strip existing controls.

## 3. Settings (current)

Sections: Recording (info) · Processing (reset affordance) · Appearance (UI radios) · Models & offline · About & copyright.

## 4. Phases

| Phase | Work |
|-------|------|
| A | Settings additive entry + sections |
| B | Record hierarchy polish |
| C | Editor preset-first chrome |
| D | Library source/derived badges |
| E | Theme applied app-wide |
| F | Full a11y pass |

## 5. Licensing

See repository root **`NOTICE.md`**: Synvidra © Innotrepid (all rights reserved by default); third-party MIT/Apache/BSD components listed with upstream links.
