#!/usr/bin/env python3
"""Inject phrase range ms fields + confirm button into Windows Captions XAML."""
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
    if "PhraseRangeStartBox" in text and "ConfirmPhraseManualRange_Click" in text:
        print("Windows phrase range XAML already present")
        return 0

    old = (
        '<Button Content="Save phrase for replacement" Click="SavePhraseReplacementRequest_Click"'
    )
    if old not in text:
        # try after Save transcript
        old2 = '<Button Content="Save transcript" Click="CreatorTranscriptSave_Click" />'
        if old2 not in text:
            raise SystemExit("Cannot find insertion point in Captions XAML")
        inject = (
            old2
            + '\n                        <TextBox x:Name="PhraseRangeStartBox" PlaceholderText="Phrase range start (ms)" Width="200" />'
            + '\n                        <TextBox x:Name="PhraseRangeEndBox" PlaceholderText="Phrase range end (ms)" Width="200" />'
            + '\n                        <Button Content="Confirm phrase range (manual)" Click="ConfirmPhraseManualRange_Click" '
            + 'ToolTipService.ToolTip="Set range on latest unverified phrase; alignmentStatus=manual" />'
        )
        text = text.replace(old2, inject, 1)
    else:
        # find end of Save phrase button tag
        idx = text.find(old)
        # end of this element
        end = text.find("/>", idx)
        if end < 0:
            end = text.find(">", idx)
            insert_at = end + 1
        else:
            insert_at = end + 2
        inject = (
            '\n                        <TextBox x:Name="PhraseRangeStartBox" PlaceholderText="Phrase range start (ms)" Width="200" />'
            '\n                        <TextBox x:Name="PhraseRangeEndBox" PlaceholderText="Phrase range end (ms)" Width="200" />'
            '\n                        <Button Content="Confirm phrase range (manual)" Click="ConfirmPhraseManualRange_Click" '
            'ToolTipService.ToolTip="Set range on latest unverified phrase; alignmentStatus=manual" />'
        )
        text = text[:insert_at] + inject + text[insert_at:]

    XAML.write_text(text, encoding="utf-8")
    print("Windows phrase range XAML injected")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
