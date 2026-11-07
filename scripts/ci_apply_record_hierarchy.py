#!/usr/bin/env python3
"""Phase B: Record hierarchy label polish — never remove controls."""
from __future__ import annotations

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

    # Only set contentDescription on known unique string literals for transport — additive attributes.
    replacements = [
        (
            'text = "Record"\n',
            'text = "Record"\n            contentDescription = "Start recording"\n',
        ),
        (
            'text = "Stop"\n',
            'text = "Stop"\n            contentDescription = "Stop recording"\n',
        ),
        (
            'text = "Pause"\n',
            'text = "Pause"\n            contentDescription = "Pause recording and teleprompter"\n',
        ),
    ]
    for old, new in replacements:
        if old in text and "contentDescription" not in text[text.find(old) : text.find(old) + len(old) + 80]:
            # Avoid double-apply
            if new not in text:
                text = text.replace(old, new, 1)

    if text != original:
        path.write_text(text, encoding="utf-8")
        print("Android: Record transport contentDescription polish")
    else:
        print("Android: no Record label anchors changed (safe no-op)")


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

    # Add AutomationProperties.Name on Record-related buttons missing names
    pairs = [
        ('Content="Record"', 'Content="Record" AutomationProperties.Name="Start recording"'),
        ('Content="Stop"', 'Content="Stop" AutomationProperties.Name="Stop recording"'),
        ('Content="Pause"', 'Content="Pause" AutomationProperties.Name="Pause recording and teleprompter"'),
    ]
    for old, new in pairs:
        if old in text and "AutomationProperties.Name" not in text[text.find(old) : text.find(old) + 120]:
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
