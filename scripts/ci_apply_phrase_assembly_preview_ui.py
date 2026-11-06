#!/usr/bin/env python3
"""Wire Captions phrase assembly preview; bootstrap body from known-good commit if needed."""
from __future__ import annotations

import importlib.util
import sys
import tempfile
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BOOTSTRAP_URL = (
    "https://raw.githubusercontent.com/innotrepid/audio-studio/"
    "ba0278c86d9bb322c249e90a49f86f9fd25e6d6d/scripts/ci_apply_phrase_assembly_preview_ui.py"
)


def _heal_windows_nullables() -> None:
    path = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml.cs"
    if not path.exists():
        return
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


def _run_bootstrap(target: str) -> None:
    raw = urllib.request.urlopen(BOOTSTRAP_URL, timeout=60).read().decode("utf-8")
    # Point ROOT at this workspace without breaking Python string quoting
    root_expr = "ROOT = Path(%r)" % str(ROOT)
    raw = raw.replace("ROOT = Path(__file__).resolve().parents[1]", root_expr, 1)
    with tempfile.NamedTemporaryFile("w", suffix=".py", delete=False, encoding="utf-8") as tmp:
        tmp.write(raw)
        tmp_path = tmp.name
    spec = importlib.util.spec_from_file_location("phrase_assembly_bootstrap", tmp_path)
    assert spec and spec.loader
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    if target in ("all", "android"):
        mod.ensure_android()
    if target in ("all", "windows"):
        mod.ensure_windows()


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    _run_bootstrap(target)
    if target in ("all", "windows"):
        _heal_windows_nullables()
    print("ci_apply_phrase_assembly_preview_ui:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
