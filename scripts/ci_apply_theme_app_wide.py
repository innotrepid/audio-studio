#!/usr/bin/env python3
"""Phase E: Theme app-wide — docs-driven; no Android MainActivity injects."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    print("Android: theme app-wide no-op (docs-only)")


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
        ('Content="System default"', 'Content="System default" AutomationProperties.Name="Theme system default"'),
        ('Content="Dark studio"', 'Content="Dark studio" AutomationProperties.Name="Theme dark studio"'),
        ('x:Name="ThemeSystemRadio"', 'x:Name="ThemeSystemRadio" AutomationProperties.Name="Theme system default"'),
        ('x:Name="ThemeDarkRadio"', 'x:Name="ThemeDarkRadio" AutomationProperties.Name="Theme dark studio"'),
    ]
    for old, new in pairs:
        idx = text.find(old)
        if idx < 0:
            continue
        if "AutomationProperties.Name" in text[idx : idx + 140] and "AutomationProperties.Name" in old:
            continue
        # Skip if already named on same element
        window = text[idx : idx + 200]
        if "AutomationProperties.Name" in window and "Theme" in window:
            continue
        if new not in text:
            text = text.replace(old, new, 1)

    if text != original:
        xaml.write_text(text, encoding="utf-8")
        print("Windows: theme radio AutomationProperties polish")
    else:
        print("Windows: no theme anchors changed (safe no-op)")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_theme_app_wide:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
