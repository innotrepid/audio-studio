#!/usr/bin/env python3
"""Phase 7.3 — accessibility labels for Creator Captions and Record teleprompter."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
    text = path.read_text(encoding="utf-8")
    changed = False

    # Studio teleprompter Start button
    if 'teleprompterStart = Button(this).apply' in text and 'contentDescription = "Start teleprompter pre-roll and recording"' not in text:
        # After first text assignment inside teleprompterStart apply, inject contentDescription
        pattern = re.compile(
            r"(val teleprompterStart = Button\(this\)\.apply \{\s*\n\s*text = \"[^\"]*\")",
            re.MULTILINE,
        )

        def inject_start(m: re.Match[str]) -> str:
            return m.group(1) + '\n            contentDescription = "Start teleprompter pre-roll and recording"'

        text2, n = pattern.subn(inject_start, text, count=1)
        if n:
            text = text2
            changed = True

    if 'teleprompterPause = Button(this).apply' in text and 'contentDescription = "Pause or resume teleprompter scroll"' not in text:
        pattern = re.compile(
            r"(val teleprompterPause = Button\(this\)\.apply \{\s*\n\s*text = \"[^\"]*\")",
            re.MULTILINE,
        )

        def inject_pause(m: re.Match[str]) -> str:
            return m.group(1) + '\n            contentDescription = "Pause or resume teleprompter scroll"'

        text2, n = pattern.subn(inject_pause, text, count=1)
        if n:
            text = text2
            changed = True

    # Scroll speed SeekBar
    if 'Teleprompter scroll speed' not in text and 'teleprompterSpeedLabel' in text:
        # Find SeekBar near teleprompterSpeedLabel
        if re.search(r"SeekBar\(this\)\.apply",
                     text[text.find('teleprompterSpeedLabel') : text.find('teleprompterSpeedLabel') + 800] if 'teleprompterSpeedLabel' in text else ''):
            pass
        idx = text.find('teleprompterSpeedLabel')
        window = text[idx : idx + 1200] if idx >= 0 else ''
        if 'SeekBar' in window and 'contentDescription = "Teleprompter scroll speed"' not in window:
            text2, n = re.subn(
                r"(SeekBar\(this\)\.apply \{)",
                r'\1\n            contentDescription = "Teleprompter scroll speed"',
                text[idx : idx + 1200],
                count=1,
            )
            if n:
                text = text[:idx] + text2 + text[idx + 1200 :]
                changed = True

    # Ensure exchangeButton helper always sets contentDescription (idempotent)
    if 'fun exchangeButton' in text and 'contentDescription = description' not in text:
        text = text.replace(
            'fun exchangeButton(label: String, description: String, action: () -> Unit) =\n'
            '            Button(this).apply { text = label; minHeight = dp(42); setOnClickListener { action() } }',
            'fun exchangeButton(label: String, description: String, action: () -> Unit) =\n'
            '            Button(this).apply { text = label; contentDescription = description; minHeight = dp(42); setOnClickListener { action() } }',
            1,
        )
        changed = True

    if changed:
        path.write_text(text, encoding="utf-8")
        print("Android phase7 accessibility wired")
    else:
        print("Android phase7 accessibility already present or anchors missing")


def ensure_windows() -> None:
    path = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml"
    if not path.exists():
        raise SystemExit(f"Missing {path}")
    text = path.read_text(encoding="utf-8")
    if "<Window" not in text:
        print("Windows XAML not restored yet; skip a11y")
        return

    # Map Content -> friendly AutomationProperties.Name for common Creator/teleprompter controls
    names = {
        "Teleprompter": "Open Creator Teleprompter workspace",
        "Captions": "Open Creator Captions workspace",
        "Scripts": "Open Creator Scripts workspace",
        "Production": "Open Creator Production workspace",
        "Generate estimated captions": "Generate estimated captions from transcript",
        "Add caption": "Add a new caption cue",
        "Save transcript": "Save transcript text",
        "Build phrase assembly preview": "Build temporary phrase assembly preview WAV",
        "Approve phrase assembly": "Approve phrase assembly and save derived WAV",
        "Discard phrase assembly": "Discard phrase assembly preview or approval",
        "Play original phrase range": "Play original phrase range from source take",
        "Play replacement take": "Play linked replacement take",
        "Stop phrase audition": "Stop phrase A/B audition playback",
        "Pre-roll + record": "Start teleprompter pre-roll and recording",
        "Pause": "Pause or resume teleprompter scroll",
        "Pause scroll": "Pause or resume teleprompter scroll",
        "Export project JSON": "Export Creator project as JSON",
        "Import project JSON": "Import Creator project from JSON",
    }

    def add_name(match: re.Match[str]) -> str:
        tag = match.group(0)
        if "AutomationProperties.Name" in tag:
            return tag
        content_m = re.search(r'Content="([^"]+)"', tag)
        if not content_m:
            return tag
        content = content_m.group(1)
        name = names.get(content)
        if not name:
            # Generic fallback for unlabeled buttons
            name = content
        if tag.endswith("/>"):
            return tag[:-2] + f' AutomationProperties.Name="{name}" />'
        if tag.endswith(">"):
            return tag[:-1] + f' AutomationProperties.Name="{name}">'
        return tag

    # Only Buttons that do not already carry AutomationProperties.Name
    text2, n = re.subn(
        r"<Button\b[^>]*Content=\"[^\"]+\"[^>]*/?>",
        add_name,
        text,
    )
    if n:
        path.write_text(text2, encoding="utf-8")
        print(f"Windows phase7 accessibility: labeled {n} buttons")
    else:
        print("Windows phase7 accessibility: no buttons patched")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_phase7_accessibility:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
