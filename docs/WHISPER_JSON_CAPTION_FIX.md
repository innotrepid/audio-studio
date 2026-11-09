# Whisper JSON / caption fix

**Symptom:** Transcribe detected language but captions/transcript showed garbage such as `)""`.

**Cause:** `nativeTranscribe` in `native_bridge_jni_rest_b.inc` built JSON with `:` + `)` instead of `:` + `"` before escaped string values (`text`, `detectedLanguage`, segment `text`, and `error`).

**Fix:** Correct raw-string delimiters so payloads are valid JSON. UI guard throws a clear `IOException` if JSON still fails to parse.

**Note:** Live Silero VAD on Record and offline Whisper on Transcribe remain separate pipelines; this fix does not merge them.
