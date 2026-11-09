# VAD + Whisper speech regions (Android)

**Goal:** Live Silero and offline Whisper cooperate for captions.

## Flow

1. **Record** with live Silero → speech regions polled on status.
2. **Stop** → regions drained; written next to the WAV **and** under `filesDir/vad-regions/` (durable index by display name).
3. **Transcribe** → resolve regions from: adjacent sidecar → durable index → last-take session list.
4. Whisper runs **per speech region**; captions use Silero `startMs`/`endMs`.
5. If no regions or all region transcripts empty → **full-file** Whisper.

## Why durable index?

After MediaStore export, the private staging WAV is deleted. Without `vad-regions/`, Library/transcribe would lose Silero timing. Durable files keep regions keyed by WAV name.

## Limits

- Matching by display name can collide if two takes share a name (UUID names avoid this).
- Tiny Whisper on short clips can still mis-hear; regions fix **timing** and skip long silence.
