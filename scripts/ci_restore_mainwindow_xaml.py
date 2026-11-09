#!/usr/bin/env python3
"""Restore platforms/windows/AudioStudio/MainWindow.xaml when truncated or wrong shell.

Loads known-good markup from SHA ebccf66 (git show or HTTPS). Does NOT inject
section-nav buttons here — those Click handlers are added by
ci_apply_section_nav.py together with code-behind methods.
"""
from __future__ import annotations

import subprocess
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
XAML_REL = "platforms/windows/AudioStudio/MainWindow.xaml"
GOOD_SHA = "ebccf66112441468d533bb80ed450c8e7257b909"
RAW_URL = (
    "https://raw.githubusercontent.com/innotrepid/audio-studio/"
    f"{GOOD_SHA}/{XAML_REL}"
)


def load_good_xaml() -> str:
    try:
        blob = subprocess.check_output(
            ["git", "show", f"{GOOD_SHA}:{XAML_REL}"],
            cwd=ROOT,
            stderr=subprocess.DEVNULL,
        )
        return blob.decode("utf-8")
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass
    try:
        subprocess.check_call(
            ["git", "fetch", "--depth=1", "origin", GOOD_SHA],
            cwd=ROOT,
            stderr=subprocess.DEVNULL,
        )
        blob = subprocess.check_output(
            ["git", "show", f"{GOOD_SHA}:{XAML_REL}"],
            cwd=ROOT,
        )
        return blob.decode("utf-8")
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass
    print(f"Fetching {RAW_URL}")
    with urllib.request.urlopen(RAW_URL, timeout=120) as resp:
        return resp.read().decode("utf-8")


def needs_restore(text: str) -> bool:
    if "<Window" not in text:
        return True
    if len(text) < 20_000:
        return True
    # Minimal shell used wrong Click names (NavRecord_Click) → runtime XamlParseException.
    if "NavRecord_Click" in text or "NavEdit_Click" in text:
        return True
    required = (
        "NavigateRecordButton",
        "RecordToggleButton",
        "PauseTeleprompterButton",
        "RecordingElapsedText",
        "TeleprompterScriptText",
    )
    return any(name not in text for name in required)


def main() -> int:
    path = ROOT / XAML_REL
    text = path.read_text(encoding="utf-8") if path.exists() else ""
    if needs_restore(text):
        print(f"Restoring {XAML_REL} from {GOOD_SHA} (was {len(text)} bytes)")
        text = load_good_xaml()
        if needs_restore(text):
            path.write_text(text, encoding="utf-8")
            raise SystemExit("Could not load a full MainWindow.xaml matching code-behind")
        path.write_text(text, encoding="utf-8")
        print("Restored bytes:", len(text))
    else:
        print(f"MainWindow.xaml already valid ({len(text)} bytes)")

    final = path.read_text(encoding="utf-8")
    if needs_restore(final):
        raise SystemExit(f"MainWindow.xaml still invalid ({len(final)} bytes)")
    if "NavRecord_Click" in final:
        raise SystemExit("MainWindow.xaml still has invalid Nav*_Click handlers")
    print("ci_restore_mainwindow_xaml: OK", len(final), "bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
