#!/usr/bin/env python3
"""Wire Captions phrase assembly preview (temp WAV, assemblyStatus=preview)."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
    text = path.read_text(encoding="utf-8")
    if "Build phrase assembly preview" in text and "fun buildPhraseAssemblyPreview" in text:
        print("Android phrase assembly preview already present")
        return
    raise SystemExit("Android assembly inject body omitted in this restore; see heal path / prior commits")


def ensure_windows() -> None:
    path = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml.cs"
    text = path.read_text(encoding="utf-8")
    if "totalFrames = _waveformTotalFrames;" in text:
        text = text.replace(
            "totalFrames = _waveformTotalFrames;",
            "totalFrames = (long)Math.Min(_waveformTotalFrames, (ulong)long.MaxValue);",
            1,
        )
        path.write_text(text, encoding="utf-8")
        print("Healed totalFrames ulong cast")
        text = path.read_text(encoding="utf-8")
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
        path.write_text(text, encoding="utf-8")
        print("Healed SampleRateHz/DurationMs nullables")
        text = path.read_text(encoding="utf-8")
    if "BuildPhraseAssemblyPreview_Click" in text:
        print("Windows phrase assembly preview already present")
        return
    raise SystemExit("Windows assembly handler missing; re-run from full apply revision ba0278c content")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_phrase_assembly_preview_ui:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
