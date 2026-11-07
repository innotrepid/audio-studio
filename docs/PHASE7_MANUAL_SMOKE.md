# Phase 7.6 — Manual smoke checklist

**Purpose:** Record evidence that Creator → Record → phrase workflows work on real hardware. CI does not replace this list.

**How to use:** Copy a section into release notes or a PR comment. Mark each item `pass` / `fail` / `n/a` and note device/OS.

## Environment

| Field | Value |
|-------|--------|
| Date | |
| Tester | |
| Branch / commit | |
| Android device + OS | |
| Windows PC + OS | |
| Build artifact | APK / self-contained publish path |

## Android

### Record + studio teleprompter

- [ ] Grant mic permission; start record; elapsed time advances
- [ ] Creator script appears on Record teleprompter after editing in Creator → Scripts
- [ ] Teleprompter settings (speed/font/spacing) from Creator affect Record view
- [ ] Start record also starts teleprompter scroll (after countdown if enabled)
- [ ] Pause pauses both capture and scroll; resume restores both
- [ ] Stop saves a playable WAV; recovery works after forced kill mid-record (if tested)

### Creator persistence

- [ ] Section create/edit survives app restart
- [ ] Export project JSON writes a file; import of that file restores title/script text
- [ ] Import of invalid JSON leaves the current project unchanged

### Captions & phrase path (if takes available)

- [ ] Generate estimated captions; provenance shows estimated
- [ ] Manual caption edit marks provenance appropriately
- [ ] Phrase request + manual range + link replacement take
- [ ] A/B audition plays original range vs replacement
- [ ] Assembly preview plays; sources unchanged on disk
- [ ] Approve writes derived WAV and sets approved; discard clears without deleting sources

### Accessibility (spot check)

- [ ] TalkBack announces Record start/stop and key Creator buttons

## Windows

### Record + Creator

- [ ] Record start/stop produces playable WAV
- [ ] Creator Scripts / Teleprompter / Captions navigation retains state
- [ ] Teleprompter settings persist across relaunch
- [ ] Export project JSON via picker; import restores project

### Phrase path

- [ ] Same A/B → preview → approve/discard sequence as Android
- [ ] Source take files unchanged after approve/discard

### Accessibility (spot check)

- [ ] Narrator reads primary Creator and Record controls; keyboard can reach them

## Packaging verification

- [ ] CI green on the release commit (Linux core, Windows core/shell, Android APK)
- [ ] APK contains `assets/models/ggml-tiny.bin` and `silero_vad.onnx` (or current pinned names)
- [ ] Windows self-contained publish folder contains `audio_studio_native.dll`, `onnxruntime.dll`, and `models/` with the same pinned models
- [ ] `scripts/verify_packaged_models.py` passes for both packages
- [ ] App opens offline with no model-download prompt

## Known limitations to restate

- Caption timings may be estimated, not audio-aligned
- Phrase ranges are manual unless a future alignment gate is enabled
- Soft crossfade is short (~15 ms); not a full mix editor
- Neural denoise is offline-only

## Sign-off

| Platform | Result | Notes |
|----------|--------|-------|
| Android | | |
| Windows | | |
| Packaging | | |

**Release claim allowed only if** all critical Record/Creator paths are `pass`, CI is green, and packaging checks pass.
