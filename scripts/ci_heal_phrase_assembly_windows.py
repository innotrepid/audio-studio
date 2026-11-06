#!/usr/bin/env python3
"""Heal Windows phrase assembly preview compile errors after apply."""
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
path = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml.cs"
if not path.exists():
    raise SystemExit(0)
text = path.read_text(encoding="utf-8")
changed = False
if "totalFrames = _waveformTotalFrames;" in text:
    text = text.replace(
        "totalFrames = _waveformTotalFrames;",
        "totalFrames = (long)Math.Min(_waveformTotalFrames, (ulong)long.MaxValue);",
        1,
    )
    changed = True
    print("Healed totalFrames ulong cast")
if "sampleRate = sourceTake.SampleRateHz;" in text:
    text = text.replace(
        "sampleRate = sourceTake.SampleRateHz;",
        "sampleRate = sourceTake.SampleRateHz ?? 0;",
        1,
    )
    text = text.replace(
        "totalFrames = (long)Math.Round(sourceTake.DurationMs * (double)sampleRate / 1000.0);",
        "totalFrames = (long)Math.Round((sourceTake.DurationMs ?? 0L) * (double)sampleRate / 1000.0);",
        1,
    )
    changed = True
    print("Healed SampleRateHz/DurationMs nullables")
if changed:
    path.write_text(text, encoding="utf-8")
else:
    print("No assembly preview heals needed")
