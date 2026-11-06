#!/usr/bin/env python3
"""Inject Save phrase for replacement button into Windows Captions XAML."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
XAML = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml"


def main() -> int:
    if len(sys.argv) > 1 and sys.argv[1] not in ("all", "windows"):
        print("skip non-windows")
        return 0
    if not XAML.exists():
        raise SystemExit(f"Missing {XAML}")
    text = XAML.read_text(encoding="utf-8")
    if "SavePhraseReplacementRequest_Click" in text:
        print("Windows phrase button already present")
        return 0
    old = '<Button Content="Save transcript" Click="CreatorTranscriptSave_Click" />'
    if old not in text:
        raise SystemExit("Cannot find Save transcript in XAML")
    text = text.replace(
        old,
        old
        + '\n                        <Button Content="Save phrase for replacement" Click="SavePhraseReplacementRequest_Click" '
        + 'ToolTipService.ToolTip="Save transcript selection as an unverified phrase request (no audio writes)" />',
        1,
    )
    XAML.write_text(text, encoding="utf-8")
    print("Windows phrase replacement button injected")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
