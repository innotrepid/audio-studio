# Record workspace hierarchy (Hybrid Studio Phase B)

**Status:** Contract on tip; label polish via CI where safe  
**Principle:** Redesign restyles and clarifies; it **never removes** Record controls, recovery, or teleprompter functions.

## Goal

One primary job: **capture a clean take** with the studio teleprompter as the hero surface, levels/clipping always visible, and post-save next steps obvious—without scrolling the Record chrome away from the transport.

## Layer order (top → bottom)

| Layer | Content | Notes |
|-------|---------|--------|
| **1. Context** | Active script/section title (from Creator) | Read-only context; edit script in Creator |
| **2. Hero** | Studio teleprompter (largest area) | Non-scrollable page chrome; teleprompter may scroll text |
| **3. Levels** | Live RMS/peak + clipping warning | Mint = healthy; warm = clip/warn |
| **4. Transport** | Record / Pause / Stop (and elapsed time) | One dominant primary action |
| **5. Secondary** | Device refresh, recovery entry, open-in-Editor / playback after save | Always reachable; not competing with Record |

Settings remains a **secondary app destination** (not a fifth Record control).

## Coupled transport (already shipped)

- Start record → start teleprompter scroll (after countdown if enabled).
- Pause → pause capture and scroll.
- Script/settings ownership: Creator → Teleprompter (`docs/CREATOR-TELEPROMPTER-FLOW.md`).

## Non-goals for Phase B

- No change to Oboe/WASAPI capture, WAV finalization, or recovery semantics.
- No removal of recovery, device picker, or post-save actions.
- No forced scrollable Record page that buries transport under content.

## Acceptance

- [ ] Teleprompter remains the largest visual block on Record.
- [ ] Meter + clipping remain visible while recording.
- [ ] Primary transport is obvious without hunting.
- [ ] All prior Record actions still present and usable.
- [ ] CI green (Android APK, Linux/Windows core).
