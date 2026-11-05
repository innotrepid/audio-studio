#!/usr/bin/env python3
"""Restore teleprompter CI apply script and patch Android prefs forward-ref safety."""
from __future__ import annotations

import runpy
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = Path(__file__).resolve()
IMPL = SCRIPT.with_name("_ci_apply_teleprompter_ui_impl.py")

# Known-good full script from this branch history (pre-placeholder)
GOOD_SHA = "2a6cc9a4ccdde16e7d2413d89d3210260c1de10a"


def _safety_patch_android_prefs_forward_refs(text: str) -> str:
    """Rewrite any early TeleprompterPreferences.save(onStop) that refs later seekbars."""
    if "var persistTeleprompterPrefs" not in text and "TeleprompterPreferences.load" in text:
        anchor = "        val teleprompterSpeedLabel = TextView(this).apply {"
        if anchor in text:
            text = text.replace(
                anchor,
                "        var persistTeleprompterPrefs: (() -> Unit)? = null\n" + anchor,
                1,
            )
            print("Safety: declared persistTeleprompterPrefs holder")

    broken = (
        "                override fun onStopTrackingTouch(seekBar: SeekBar?) {\n"
        "                    TeleprompterPreferences.save(\n"
        "                        this@MainActivity,\n"
        "                        teleprompterSpeed.progress,\n"
        "                        teleprompterFontSize.progress,\n"
        "                        teleprompterLineSpacing.progress,\n"
        "                        teleprompterCountdown.selectedItemPosition,\n"
        "                    )\n"
        "                }\n"
    )
    safe = (
        "                override fun onStopTrackingTouch(seekBar: SeekBar?) {\n"
        "                    persistTeleprompterPrefs?.invoke()\n"
        "                }\n"
    )
    if broken in text:
        text = text.replace(broken, safe)
        print("Safety: replaced early save onStop with late-bound invoke")

    if "persistTeleprompterPrefs = {" not in text and "TeleprompterPreferences.load" in text:
        bind = (
            "        persistTeleprompterPrefs = {\n"
            "            TeleprompterPreferences.save(\n"
            "                this@MainActivity,\n"
            "                teleprompterSpeed.progress,\n"
            "                teleprompterFontSize.progress,\n"
            "                teleprompterLineSpacing.progress,\n"
            "                teleprompterCountdown.selectedItemPosition,\n"
            "            )\n"
            "        }\n"
        )
        mark = "        val teleprompterControls = LinearLayout(this).apply {"
        load_idx = text.find("val teleprompterPrefs = TeleprompterPreferences.load")
        ctrl_idx = text.find(mark)
        if load_idx > 0 and ctrl_idx > load_idx:
            text = text[:ctrl_idx] + bind + text[ctrl_idx:]
            print("Safety: bound persistTeleprompterPrefs after load")
    return text


def main() -> int:
    blob = subprocess.check_output(
        ["git", "show", f"{GOOD_SHA}:scripts/ci_apply_teleprompter_ui.py"],
        cwd=ROOT,
    )
    text = blob.decode("utf-8")
    if text.strip() in ("PLACEHOLDER", "PLACEHOLDER_TOO_LARGE_USE_CREATE") or len(text) < 500:
        raise SystemExit("restored script still placeholder")

    # Ensure safety patch is applied inside ensure_android_mainactivity of the restored script
    if "_safety_patch_android_prefs_forward_refs" not in text:
        # Inject safety function before ensure_android_mainactivity
        safety_src = Path(__file__).read_text(encoding="utf-8")
        # Extract just the safety function from this bootstrap
        start = safety_src.find("def _safety_patch_android_prefs_forward_refs")
        end = safety_src.find("\ndef main()", start)
        safety_fn = safety_src[start:end]
        text = text.replace(
            "def ensure_android_mainactivity():",
            safety_fn + "\ndef ensure_android_mainactivity():",
            1,
        )
        if "text = wire_android_teleprompter_prefs(text)" in text:
            text = text.replace(
                "    text = wire_android_teleprompter_prefs(text)\n",
                "    text = wire_android_teleprompter_prefs(text)\n"
                "    text = _safety_patch_android_prefs_forward_refs(text)\n",
                1,
            )
        else:
            text = text.replace(
                "    text = fix_android_forward_refs(text)\n",
                "    text = fix_android_forward_refs(text)\n"
                "    if \"def wire_android_teleprompter_prefs\" in open(__file__, encoding=\"utf-8\").read():\n"
                "        text = wire_android_teleprompter_prefs(text)\n"
                "    text = _safety_patch_android_prefs_forward_refs(text)\n",
                1,
            )
        print("Injected forward-ref safety into restored apply script")

    IMPL.write_text(text, encoding="utf-8")
    sys.argv[0] = str(IMPL)
    runpy.run_path(str(IMPL), run_name="__main__")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
