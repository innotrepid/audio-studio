#!/usr/bin/env python3
"""Inject Windows Captions phrase assembly discard button."""
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
    if "DiscardPhraseAssembly_Click" in text:
        print("Windows assembly discard button already present")
        return 0

    markers = [
        'Click="ApprovePhraseAssembly_Click"',
        'Click="BuildPhraseAssemblyPreview_Click"',
        'Click="StopPhraseAudition_Click"',
    ]
    for m in markers:
        idx = text.find(m)
        if idx < 0:
            continue
        end = text.find("/>", idx)
        if end < 0:
            end = text.find(">", idx)
            insert_at = end + 1
        else:
            insert_at = end + 2
        inject = (
            '\n                        <Button Content="Discard phrase assembly" '
            'Click="DiscardPhraseAssembly_Click" '
            'ToolTipService.ToolTip="assemblyStatus=discarded; clear derivedPath" />'
        )
        text = text[:insert_at] + inject + text[insert_at:]
        XAML.write_text(text, encoding="utf-8")
        print("Windows assembly discard button injected after", m)
        return 0

    raise SystemExit("Cannot find Captions insertion point for assembly discard")


if __name__ == "__main__":
    raise SystemExit(main())
