# Hybrid Studio UI — Implementation Plan

**Status:** Planning + Settings surface exposed (UI)  
**Branch:** `Redesign,-Teleprompter,-captions-&-more`  
**Scope:** Visual/UX structure only in this slice; audio engine behavior unchanged

## 1. What modern hybrid studios optimize for

Patterns observed across creator tools (Descript-style text+timeline editors, Riverside studio layouts, CapCut media+timeline shells, local voice studios, podcast recorders):

| Pattern | Product intent | Synvidra mapping |
|---------|----------------|------------------|
| **Primary task destinations** | Few top-level modes; avoid a DAW full of peer tabs | Record · Editor · Creator · Library |
| **Secondary settings** | Account/prefs off the main path (Descript Settings) | Settings gear / secondary entry—not a 5th tab |
| **One hero surface** | Waveform, teleprompter, or canvas dominates | Record: teleprompter; Editor: waveform |
| **Presets before pro** | CapCut/Descript expose simple actions first | Processing presets → advanced collapsed |
| **Derived vs source** | Clear original vs edited | Always derived WAV; never silent overwrite |
| **Dark studio chrome** | Near-black, high-contrast meters, restrained accents | Charcoal + blue accent + mint live |
| **State survival** | Navigate without killing capture/edit context | Keep screen instances / loaded selection |
| **Accessibility** | VoiceOver/TalkBack, larger text, keyboard (ZeroStudio, Backpack) | Labels shipped; deeper pass ongoing |

Hybrid means: **Creator-friendly capture and script flow** in the same app as **precise waveform tools**, without dumping every control on every screen.

## 2. Target information architecture

```text
┌─────────────────────────────────────────────────────────┐
│  [Record]  [Editor]  [Creator]  [Library]     ⚙ Settings │
└─────────────────────────────────────────────────────────┘
        │         │         │           │            │
        │         │         │           │            └─ App prefs (secondary)
        │         │         │           └─ Takes / derived / search
        │         │         └─ Scripts · Teleprompter · Captions · Production
        │         └─ Waveform · Trim · Process · A/B
        └─ Capture · Meter · Studio teleprompter · Recover
```

### Settings (this implementation slice)

Settings is **reachable and visible**, organized into sections. Only **processing preference** controls bind to existing persistence (`docs/SETTINGS-PREFERENCE-CONTRACT.md`). Other rows are structured UI for later wiring:

| Section | UI content | Backend status |
|---------|------------|----------------|
| **Recording** | Default sample-rate note, input-device help text | Display / placeholder |
| **Processing** | Threshold, attenuation, gain, high-pass, preset, Reset to Natural | **Persisted** (existing contract) |
| **Appearance** | Theme: System / Dark (UI selection) | Local UI state only for now |
| **Models & offline** | Model names/status copy (bundled offline) | Read-only informational |
| **About** | Product name Synvidra, local-first note | Static |

Non-goals for this slice: cloud account, theme applied to full chrome, changing live capture sample rate, deleting library roots.

## 3. Visual system (implementation rules)

1. **Surfaces:** opaque charcoal panels; no glass stacks.
2. **Accent:** blue for selected nav and primary buttons.
3. **Live:** mint only for healthy meter / recording-active.
4. **Warn:** amber/red only for clip / destructive.
5. **Type:** clear section headers (11–12sp / small caps style), body readable at system font scale.
6. **Density:** Settings uses grouped lists, not a wall of equal buttons.

## 4. Platform layout

### Android

- Bottom nav remains Record · Editor · Creator · Library.
- **Settings** opened from a header/action control (and/or overflow)—not a fifth tab.
- Settings is a full-screen vertical scroll of section cards.
- Back returns to the previous primary destination; does not tear down Record session state if the activity is retained.

### Windows

- Left rail: Record · Editor · Creator · Library.
- Settings as rail footer item or title-bar command.
- Wider two-column optional later; v1 is a single scroll column matching Android sections.

## 5. Phased UI delivery (after Settings exposure)

| Phase | Work | Exit |
|-------|------|------|
| **A** | Settings screen + nav entry; processing prefs bound | User can open Settings and reset Natural |
| **B** | Record hierarchy polish (post-save actions, meter prominence) | Phase 1 Record items |
| **C** | Editor selection chrome + preset-first processing panel | Phase 1 Editor items |
| **D** | Library source vs derived badges | Phase 1 Library |
| **E** | Appearance theme applied app-wide | Theme preference persists |
| **F** | Full keyboard / TalkBack pass on Settings + Record | Phase 7 deeper a11y |

## 6. Engineering constraints

- Do not change Oboe/WASAPI capture semantics in UI-only PRs.
- Do not overwrite source recordings.
- Prefer CI apply scripts for large `MainActivity.kt` / `MainWindow` edits until permanent source lands.
- CI: Android APK + Windows shell + Linux core green.

## 7. Acceptance (this PR)

- [x] Planning doc on branch reflecting modern hybrid patterns.
- [x] Settings reachable on Android and Windows.
- [x] Settings UI sections as above; processing controls use existing preference persistence where available.
- [ ] Theme actually restyles chrome (deferred).
- [ ] Manual smoke of Settings open/close on device/desktop.

## References (research inputs)

- Text-first + properties panels (Descript-style product surfaces).
- Studio layout tools and scene-focused chrome (Riverside).
- Media library + timeline + simple audio controls (CapCut patterns).
- Local-first voice workspace and model visibility (open local voice studios).
- Accessibility-first recording apps (larger text, screen-reader actions).
- Existing Synvidra docs: `SCREEN_ARCHITECTURE.md`, `UI-ROADMAP.md`, `CREATOR_STUDIO_REDESIGN.md`, `SETTINGS-PREFERENCE-CONTRACT.md`.
