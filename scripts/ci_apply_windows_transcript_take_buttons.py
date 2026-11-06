#!/usr/bin/env python3
"""Inject Windows Captions buttons for transcriptTakeId link/clear."""
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
    if "LinkTranscriptToFirstTake_Click" in text:
        print("Windows transcript-take buttons already present")
        return 0

    old = (
        '<Button Content="Save transcript" Click="CreatorTranscriptSave_Click" />\n'
        '                        <Button Content="Copy selected script" Click="CreatorTranscriptCopyScript_Click" />'
    )
    new = (
        '<Button Content="Save transcript" Click="CreatorTranscriptSave_Click" />\n'
        '                        <Button Content="Link transcript to first take" Click="LinkTranscriptToFirstTake_Click" '
        'ToolTipService.ToolTip="Associate transcriptTakeId with the first linked take (not timed alignment)" />\n'
        '                        <Button Content="Clear transcript take link" Click="ClearTranscriptTakeLink_Click" '
        'ToolTipService.ToolTip="Remove transcriptTakeId from the project" />\n'
        '                        <Button Content="Copy selected script" Click="CreatorTranscriptCopyScript_Click" />'
    )
    if old not in text:
        # tolerant spacing
        alt_old = (
            '<Button Content="Save transcript" Click="CreatorTranscriptSave_Click" />'
        )
        if alt_old not in text:
            raise SystemExit("Cannot find Save transcript button in MainWindow.xaml")
        text = text.replace(
            alt_old,
            alt_old
            + '\n                        <Button Content="Link transcript to first take" Click="LinkTranscriptToFirstTake_Click" '
            + 'ToolTipService.ToolTip="Associate transcriptTakeId with the first linked take (not timed alignment)" />'
            + '\n                        <Button Content="Clear transcript take link" Click="ClearTranscriptTakeLink_Click" '
            + 'ToolTipService.ToolTip="Remove transcriptTakeId from the project" />',
            1,
        )
    else:
        text = text.replace(old, new, 1)

    XAML.write_text(text, encoding="utf-8")
    print("Windows transcript-take buttons injected into MainWindow.xaml")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
