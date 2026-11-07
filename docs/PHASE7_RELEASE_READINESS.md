# Phase 7 — Cross-platform polish and release readiness

**Status:** Documentation complete through 7.6 checklist; **manual evidence still required** before release claims  
**Branch:** `Redesign,-Teleprompter,-captions-&-more`

## Slices

| Slice | Outcome | Status |
|-------|---------|--------|
| 7.1 Soft crossfade | ~15 ms linear joins | Done |
| 7.2 Docs sync | ROADMAP + redesign status | Done |
| 7.3 Accessibility | Teleprompter + Captions labels | Done |
| 7.4 Project backup/restore | Import/export JSON + tests | Done |
| 7.5 Changelog + README | Product docs match tip | Done |
| 7.6 Manual smoke + packaging notes | Checklist for device/desktop + models | **This change** |

## Packaging notes

- Models are fetched and checksum-verified in CI (`scripts/fetch_transcription_models.py`).
- Android: embed under APK `assets/models/`; verify with `scripts/verify_packaged_models.py --apk …`.
- Windows: copy into publish `models/` next to `audio_studio_native.dll` and `onnxruntime.dll`; verify with `--directory …`.
- Runtime must resolve bundled model paths offline; do not require post-install downloads for core VAD/transcription models.

## Manual smoke

Use [PHASE7_MANUAL_SMOKE.md](PHASE7_MANUAL_SMOKE.md). Attach filled results to the release PR or tag notes.

## Exit gate

Android and Windows workflows usable end to end; CI green; **manual test evidence recorded**; known limitations documented.
