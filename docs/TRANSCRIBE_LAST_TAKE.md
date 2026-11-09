# Transcribe last take (Android)

After a recording is finalized and verified, `latestVerifiedTakeUri` is set and VAD regions (if any) are in `lastTakeVadRegions` / durable index.

Tapping **Transcribe WAV** offers:

1. **Last take** — runs Whisper immediately (per Silero region when regions exist)
2. **Pick file** — existing document picker

Status after finalize reminds the user to use Transcribe WAV for Silero-timed captions.
