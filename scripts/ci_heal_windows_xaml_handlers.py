#!/usr/bin/env python3
"""Remove XAML Click= handlers that have no matching method in MainWindow.xaml.cs.

WinUI resolves Click at LoadComponent time. Missing methods → XamlParseException
even when MSBuild succeeds. This must run after all Windows XAML/CS inject steps.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
XAML = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml"
CS = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml.cs"


def main() -> int:
    if not XAML.exists() or not CS.exists():
        raise SystemExit("MainWindow.xaml or .xaml.cs missing")

    xaml = XAML.read_text(encoding="utf-8")
    cs = CS.read_text(encoding="utf-8")

    if "<Window" not in xaml or len(xaml) < 20_000:
        raise SystemExit(
            f"MainWindow.xaml still not restored ({len(xaml)} bytes). "
            "Run ci_restore_mainwindow_xaml.py first."
        )

    # Methods that can be used as Click handlers
    handlers = set(re.findall(r"\b([A-Za-z_][A-Za-z0-9_]*_Click)\s*\(", cs))
    handlers |= set(re.findall(r"\b([A-Za-z_][A-Za-z0-9_]*_Click)\s*=>", cs))

    clicks = set(re.findall(r'Click="([A-Za-z_][A-Za-z0-9_]*)"', xaml))
    missing = sorted(clicks - handlers)

    if not missing:
        print("ci_heal_windows_xaml_handlers: all", len(clicks), "Click handlers resolved")
        return 0

    print("Orphan Click handlers (will strip controls):", ", ".join(missing))

    # Remove elements that reference missing handlers (Button/self-closing or with body)
    original = xaml
    for name in missing:
        # Self-closing tag containing this Click
        pattern_sc = re.compile(
            r"\s*<[A-Za-z][A-Za-z0-9.]*\b[^>]*Click=\""
            + re.escape(name)
            + r"\"[^>]*/>",
            re.MULTILINE,
        )
        xaml, n1 = pattern_sc.subn("", xaml)
        # Open+close pair on one line or short block
        pattern_pair = re.compile(
            r"\s*<[A-Za-z][A-Za-z0-9.]*\b[^>]*Click=\""
            + re.escape(name)
            + r"\"[^>]*>.*?</[A-Za-z][A-Za-z0-9.]*>",
            re.MULTILINE | re.DOTALL,
        )
        xaml, n2 = pattern_pair.subn("", xaml)
        print(f"  stripped {name}: self-close={n1} paired={n2}")

    # Also reject known-bad minimal shell names if somehow present
    if "NavRecord_Click" in xaml:
        raise SystemExit("NavRecord_Click still present after heal")

    remaining = set(re.findall(r'Click="([A-Za-z_][A-Za-z0-9_]*)"', xaml))
    still = sorted(remaining - handlers)
    if still:
        raise SystemExit(f"Could not strip orphan handlers: {still}")

    if xaml == original:
        raise SystemExit("Orphans detected but no XAML changes applied")

    XAML.write_text(xaml, encoding="utf-8")
    print("ci_heal_windows_xaml_handlers: wrote", XAML, "bytes", len(xaml))
    print("Remaining Click handlers:", len(remaining))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
