# Live VAD diagnostics (Android)

**Context:** Users reported Silero VAD status appearing only once. Live VAD is separate from Whisper transcription.

## Behavior after this change

| Moment | Status |
|--------|--------|
| Record start (model OK) | `Recording · live Silero VAD listening for speech endpoints` |
| Record start (no model) | Explains VAD unavailable + model error |
| While recording, no closed region yet | Same listening line (poll) |
| Region closed | `Recording · VAD regions N · last ends …s` |
| Poll exception | Surfaces message; capture continues |
| Stop | Drains pending regions; diagnostics include `vad_regions` / `vad_summary` |

Regions only appear when speech is followed by enough silence (Silero endpointing). Continuous speech without a pause may show a single region at stop after `finish()`.

## Not included

- Feeding VAD regions into Whisper (still a separate product step).
