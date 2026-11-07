#!/usr/bin/env python3
"""Phase B: Record hierarchy polish — docs-driven; Android code no-op.

A previous Android pass inserted `contentDescription = ...` next to
`text = "Record"` outside a View.apply block, causing:
  Unresolved reference 'contentDescription'
Android therefore only strips any such orphan lines. Windows may still set
AutomationProperties.Name on XAML Content= attributes when safe.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
    if not path.exists():
        print("Android MainActivity missing; skip")
        return
    text = path.read_text(encoding="utf-8")
    original = text

    # Remove orphan contentDescription lines that are not inside a clear apply block context.
    # Only strip lines we may have injected in the failed hierarchy pass.
    text = re.sub(
        r"\n\s*contentDescription = \"Start recording\"\n",
        "\n",
        text,
    )
    text = re.sub(
        r"\n\s*contentDescription = \"Stop recording\"\n",
        "\n",
        text,
    )
    text = re.sub(
        r"\n\s*contentDescription = \"Pause recording and teleprompter\"\n",
        "\n",
        text,
    )

    if text != original:
        path.write_text(text, encoding="utf-8")
        print("Android: stripped orphan Record contentDescription lines")
    else:
        print("Android: Record hierarchy no-op (docs-only)")


def ensure_windows() -> None:
    xaml = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml"
    if not xaml.exists():
        print("Windows XAML missing; skip")
        return
    text = xaml.read_text(encoding="utf-8")
    if "<Window" not in text:
        print("Windows XAML not restored; skip")
        return
    original = text

    pairs = [
        ('Content="Record"', 'Content="Record" AutomationProperties.Name="Start recording"'),
        ('Content="Stop"', 'Content="Stop" AutomationProperties.Name="Stop recording"'),
        ('Content="Pause"', 'Content="Pause" AutomationProperties.Name="Pause recording and teleprompter"'),
    ]
    for old, new in pairs:
        idx = text.find(old)
        if idx < 0:
            continue
        window = text[idx : idx + 160]
        if "AutomationProperties.Name" in window:
            continue
        if new not in text:
            text = text.replace(old, new, 1)

    if text != original:
        xaml.write_text(text, encoding="utf-8")
        print("Windows: Record transport AutomationProperties polish")
    else:
        print("Windows: no Record label anchors changed (safe no-op)")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_record_hierarchy:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
