#!/usr/bin/env python3
"""Phase F: Full a11y pass — docs + safe Windows Names; Android no MainActivity inject."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    # Phase 7 + prior phases already label where safe; no new MainActivity injects.
    print("Android: a11y full pass no-op (docs + existing Phase 7 apply)")


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
        ('Content="Record"', 'Content="Record" AutomationProperties.Name="Record workspace"'),
        ('Content="Editor"', 'Content="Editor" AutomationProperties.Name="Editor workspace"'),
        ('Content="Creator"', 'Content="Creator" AutomationProperties.Name="Creator workspace"'),
        ('Content="Settings"', 'Content="Settings" AutomationProperties.Name="Open Settings"'),
    ]
    for old, new in pairs:
        idx = text.find(old)
        if idx < 0:
            continue
        if "AutomationProperties.Name" in text[idx : idx + 160]:
            continue
        if new not in text:
            text = text.replace(old, new, 1)

    if text != original:
        xaml.write_text(text, encoding="utf-8")
        print("Windows: primary nav AutomationProperties polish")
    else:
        print("Windows: no nav anchors changed (safe no-op)")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_a11y_full_pass:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
