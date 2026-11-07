# App-wide theme (Hybrid Studio Phase E)

**Status:** Contract on tip  
**Principle:** Theme restyles chrome; it **never removes** destinations, controls, or functions.

## Goal

A consistent **dark studio** look across Record, Editor, Creator, Library, and Settings, with an explicit preference for **System default** vs **Dark studio** (Settings Appearance section).

## Token story

| Role | Intent |
|------|--------|
| **Background** | Charcoal / near-black panels |
| **Surface** | Slightly elevated panels for cards and lists |
| **Accent** | Blue for primary actions |
| **Live / healthy** | Mint for level meters and positive status |
| **Warn / clip** | Warm amber/red for clipping and errors |
| **Text** | High-contrast primary; muted secondary |

Exact hex values may already exist in platform code; this phase **documents** the shared language and preference surface rather than forcing a one-shot color rewrite that risks compile breaks.

## Preference

| Choice | Behavior |
|--------|----------|
| **System default** | Follow OS light/dark when platform supports it |
| **Dark studio** | Force studio dark chrome |

Persistence belongs with app preferences (see Settings / preference contracts). Appearance radios in Settings are the UI entry; wiring may be incremental per platform.

## Non-goals

- No removal of workspaces or controls.
- No Android mid-file theme engine inject into `MainActivity.kt`.
- No change to capture, DSP, or file safety.

## Acceptance

- [ ] Product docs describe System vs Dark studio.
- [ ] Settings Appearance section remains the theme control surface.
- [ ] CI green.
