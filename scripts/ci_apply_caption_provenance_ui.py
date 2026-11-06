#!/usr/bin/env python3
"""Mark manual captions and show timing provenance in caption lists."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
    text = path.read_text(encoding="utf-8")
    original = text
    text = text.replace(
        "CreatorProjectStore.Caption(UUID.randomUUID().toString(),start.text.toString().toLong(),end.text.toString().toLong(),text.text.toString().trim())",
        "CreatorProjectStore.Caption(UUID.randomUUID().toString(),start.text.toString().toLong(),end.text.toString().toLong(),text.text.toString().trim(),CreatorProjectStore.Caption.PROVENANCE_MANUAL)",
    )
    text = text.replace(
        "                                                                        text = text\n                                                                    ))",
        "                                                                        text = text,\n                                                                        timingProvenance = CreatorProjectStore.Caption.PROVENANCE_MANUAL,\n                                                                    ))",
    )
    text = text.replace(
        'text = "${index+1}. ${caption.startMs}\u2013${caption.endMs} ms"',
        'text = "${index+1}. ${caption.startMs}\u2013${caption.endMs} ms \u00b7 ${caption.timingProvenance}"',
    )
    if text != original:
        path.write_text(text, encoding="utf-8")
        print("Android caption provenance UI wired")
    else:
        print("Android caption provenance UI unchanged")


def ensure_windows() -> None:
    path = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml.cs"
    text = path.read_text(encoding="utf-8")
    original = text

    def add_manual(match: re.Match[str]) -> str:
        block = match.group(0)
        if "TimingProvenance" in block:
            return block
        return block[:-1] + "\n                TimingProvenance = CreatorProjectStore.CaptionDocument.ProvenanceManual,\n            }"

    text = re.sub(
        r"new CreatorProjectStore\.CaptionDocument\s*\{[^}]+\}",
        add_manual,
        text,
        count=8,
    )
    text = text.replace(
        'caption.StartMs + "\u2013" + caption.EndMs + " ms"',
        'caption.StartMs + "\u2013" + caption.EndMs + " ms \u00b7 " + caption.TimingProvenance',
    )
    if text != original:
        path.write_text(text, encoding="utf-8")
        print("Windows caption provenance UI wired")
    else:
        print("Windows caption provenance UI unchanged")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_caption_provenance_ui:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
