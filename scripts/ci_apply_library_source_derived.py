#!/usr/bin/env python3
"""Phase D: Library source/derived — docs-driven; no Android MainActivity injects."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    print("Android: Library source/derived no-op (docs-only)")


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
        ('Content="Library"', 'Content="Library" AutomationProperties.Name="Library workspace"'),
        ('Header="Library"', 'Header="Library" AutomationProperties.Name="Library files"'),
    ]
    for old, new in pairs:
        idx = text.find(old)
        if idx < 0:
            continue
        if "AutomationProperties.Name" in text[idx : idx + 120]:
            continue
        if new not in text:
            text = text.replace(old, new, 1)

    if text != original:
        xaml.write_text(text, encoding="utf-8")
        print("Windows: Library AutomationProperties polish")
    else:
        print("Windows: no Library anchors changed (safe no-op)")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_library_source_derived:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
