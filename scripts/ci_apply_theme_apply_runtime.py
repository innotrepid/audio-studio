#!/usr/bin/env python3
"""Wire StudioTheme.applyWindowChrome into MainActivity + Settings onThemeChanged."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"


def ensure_android() -> None:
    if not MAIN.exists():
        print("MainActivity missing; skip")
        return
    text = MAIN.read_text(encoding="utf-8")
    original = text

    # 1) Apply chrome early in onCreate (after super + diagnostics is fine; before root is ideal)
    early = (
        "        super.onCreate(savedInstanceState)\n"
        "        diagnosticsCapture = DiagnosticsCapture(this)\n"
    )
    early_new = (
        "        super.onCreate(savedInstanceState)\n"
        "        StudioTheme.applyWindowChrome(this)\n"
        "        diagnosticsCapture = DiagnosticsCapture(this)\n"
    )
    if "StudioTheme.applyWindowChrome(this)" not in text:
        if early not in text:
            raise SystemExit("onCreate early anchor not found")
        text = text.replace(early, early_new, 1)

    # 2) Root content background uses resolved palette
    root_bg = "            setBackgroundColor(Color.rgb(16, 18, 24))\n"
    root_bg_new = "            setBackgroundColor(StudioTheme.resolvedPalette(this@MainActivity).windowBackground)\n"
    if "StudioTheme.resolvedPalette" not in text:
        if root_bg not in text:
            # may already be patched differently
            print("root background anchor missing or already themed")
        else:
            text = text.replace(root_bg, root_bg_new, 1)

    # 3) SettingsScreen.build — add onThemeChanged if not present
    # After real-UI wiring the call looks like:
    old_build = (
        "                SettingsScreen.build(\n"
        "                    this@MainActivity,\n"
        "                    resources.displayMetrics.density,\n"
        "                    onResetProcessingToNatural = { applyProcessingPreset(\"Natural\") },\n"
        "                ),\n"
    )
    new_build = (
        "                SettingsScreen.build(\n"
        "                    this@MainActivity,\n"
        "                    resources.displayMetrics.density,\n"
        "                    onResetProcessingToNatural = { applyProcessingPreset(\"Natural\") },\n"
        "                    onThemeChanged = {\n"
        "                        StudioTheme.applyWindowChrome(this@MainActivity)\n"
        "                        root.setBackgroundColor(StudioTheme.resolvedPalette(this@MainActivity).windowBackground)\n"
        "                    },\n"
        "                ),\n"
    )
    if "onThemeChanged" not in text:
        if old_build not in text:
            print("SettingsScreen.build anchor not found (run real-UI apply first)")
        else:
            text = text.replace(old_build, new_build, 1)

    if text == original:
        print("Theme apply runtime: no changes (already applied or anchors skipped)")
        return
    MAIN.write_text(text, encoding="utf-8")
    print("Theme apply runtime wired into MainActivity")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "android"
    if target in ("all", "android"):
        ensure_android()
    print("ci_apply_theme_apply_runtime:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
