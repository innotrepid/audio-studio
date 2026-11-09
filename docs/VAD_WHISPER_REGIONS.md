# VAD + Whisper speech regions (Android)

**Goal:** Live Silero and offline Whisper cooperate for captions.

## Flow

1. **Record** with live Silero → speech regions polled on status.
2. **Stop** → regions drained; written as `{wav}.vad.json` sidecar via `VadRegionStore`; kept in `lastTakeVadRegions` for the session.
3. **Transcribe** that take → if regions exist, Whisper runs **per speech region**; captions use Silero `startMs`/`endMs`.
4. If no regions or all region transcripts empty → **full-file** Whisper (previous behavior).

## Files

| Piece | Role |
|-------|------|
| `VadRegionStore.kt` | Read/write sidecar JSON |
| `ci_apply_vad_whisper_regions.py` | MainActivity wiring |

## Limits

- Sidecar is best-effort for same-session / app-private paths; public MediaStore exports may not carry the JSON until a later export step.
- Tiny Whisper on short clips can still mis-hear; regions only fix **timing structure** and silence skipping.
