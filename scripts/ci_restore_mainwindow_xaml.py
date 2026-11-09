#!/usr/bin/env python3
"""Restore platforms/windows/AudioStudio/MainWindow.xaml when truncated.

The section-nav merge left a stub XAML on Redesign. Teleprompter UI apply
expects a full Window document. Restore from the last known-good commit
(via git show or HTTPS raw) and optionally inject Record section-nav controls.
"""
from __future__ import annotations

import subprocess
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
XAML_REL = "platforms/windows/AudioStudio/MainWindow.xaml"
# Pre-section-nav Redesign tip with a full MainWindow.xaml
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
    with urllib.request.urlopen(RAW_URL, timeout=60) as resp:
        return resp.read().decode("utf-8")


def main() -> int:
    path = ROOT / XAML_REL
    text = path.read_text(encoding="utf-8") if path.exists() else ""
    if "<Window" not in text or "PauseTeleprompterButton" not in text or len(text) < 10_000:
        print(f"Restoring {XAML_REL} from {GOOD_SHA} (was {len(text)} bytes)")
        text = load_good_xaml()
        if "<Window" not in text or len(text) < 10_000:
            raise SystemExit("Could not load a full MainWindow.xaml")
        path.write_text(text, encoding="utf-8")
        print("Restored bytes:", len(text))

    if "TeleprompterPrevSectionButton" not in text:
        old = (
            '                            <Button x:Name="PauseTeleprompterButton" Content="Pause" '
            'Click="PauseTeleprompter_Click" IsEnabled="False"/>\n'
            '                            <ComboBox x:Name="TeleprompterCountdownComboBox"'
        )
        new = (
            '                            <Button x:Name="PauseTeleprompterButton" Content="Pause" '
            'Click="PauseTeleprompter_Click" IsEnabled="False"/>\n'
            '                            <Button x:Name="TeleprompterPrevSectionButton" Content="\u25c0 Section" '
            'Click="TeleprompterPrevSection_Click" ToolTipService.ToolTip="Previous script section"/>\n'
            '                            <TextBlock x:Name="TeleprompterSectionLabel" Text="Section" '
            'VerticalAlignment="Center" Foreground="#AEB9CC" FontSize="12" MinWidth="120"/>\n'
            '                            <Button x:Name="TeleprompterNextSectionButton" Content="Section \u25b6" '
            'Click="TeleprompterNextSection_Click" ToolTipService.ToolTip="Next script section"/>\n'
            '                            <ComboBox x:Name="TeleprompterCountdownComboBox"'
        )
        if old not in text:
            raise SystemExit("Restored XAML is missing PauseTeleprompterButton row")
        path.write_text(text.replace(old, new, 1), encoding="utf-8")
        print("Injected section-nav buttons into MainWindow.xaml")
    else:
        print("MainWindow.xaml already has section-nav buttons")

    # Final size check
    final = path.read_text(encoding="utf-8")
    if len(final) < 10_000 or "<Window" not in final:
        raise SystemExit(f"MainWindow.xaml still invalid ({len(final)} bytes)")
    print("ci_restore_mainwindow_xaml: OK", len(final), "bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
