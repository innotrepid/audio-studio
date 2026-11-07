#!/usr/bin/env python3
"""Phase C: Editor preset-first — docs-driven; no Android MainActivity injects."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    # Intentionally no MainActivity mutation (prior injects caused compile failures).
    print("Android: Editor preset-first no-op (docs-only)")


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

    # Prefer labeling preset buttons if present; never remove controls.
    pairs = [
        ('Content="Natural"', 'Content="Natural" AutomationProperties.Name="Processing preset Natural"'),
        ('Content="Broadcast"', 'Content="Broadcast" AutomationProperties.Name="Processing preset Broadcast"'),
        ('Content="Podcast"', 'Content="Podcast" AutomationProperties.Name="Processing preset Podcast"'),
        ('Content="Reset processing"', 'Content="Reset processing" AutomationProperties.Name="Reset processing to Natural"'),
    ]
    for old, new in pairs:
        idx = text.find(old)
        if idx < 0:
            continue
        if "AutomationProperties.Name" in text[idx : idx + 180]:
            continue
        if new not in text:
            text = text.replace(old, new, 1)

    if text != original:
        xaml.write_text(text, encoding="utf-8")
        print("Windows: processing preset AutomationProperties polish")
    else:
        print("Windows: no preset anchors changed (safe no-op)")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_editor_preset_first:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
