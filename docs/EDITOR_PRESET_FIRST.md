# Editor + Processing hierarchy (Hybrid Studio Phase C)

**Status:** Contract on tip  
**Principle:** Redesign clarifies hierarchy; it **never removes** Editor or Processing controls, A/B compare, or non-destructive export paths.

## Goal

Make **presets the first decision** for cleanup, with advanced sliders available but secondary—matching modern hybrid studios (Descript-style “pick a look,” then tweak).

## Layer order (Editor workspace)

| Layer | Content | Notes |
|-------|---------|--------|
| **1. Context** | Open file name + source vs derived badge | Source never silently overwritten |
| **2. Hero** | Waveform + selection handles | Primary visual for trim/export |
| **3. Transport / selection** | Play, A/B, selection in/out, export | Explicit actions |
| **4. Processing — presets** | Natural / Broadcast / Podcast / Custom | **First** processing choice |
| **5. Processing — advanced** | Threshold, attenuation, gain, high-pass, reset | Secondary; already on preference contract |

Processing may live on Editor or as an attached panel; it must **not** become a fifth primary nav tab.

## Behavior (already partially shipped)

- Processing prefs persist (`docs/SETTINGS-PREFERENCE-CONTRACT.md`).
- Natural reset remains available.
- Export / process always produces **derived** audio when writing processed output.

## Non-goals for Phase C

- No DSP algorithm changes.
- No removal of advanced controls.
- No Android mid-file UI injects (compile-safe; labels via docs until safe anchors exist).

## Acceptance

- [ ] Preset chips/buttons remain the first processing controls in the layout story.
- [ ] Advanced controls still present and usable.
- [ ] A/B and source/derived semantics unchanged.
- [ ] CI green.
