#!/usr/bin/env python3
"""Wire SettingsScreen into MainActivity navigation (surgical, unique anchors)."""
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
    if "SettingsScreen.build" in text and 'navButton("Settings"' in text:
        print("Android Settings real UI already wired")
        return

    original = text

    # 1) Create settingsSection after librarySection makeSection block
    lib_anchor = (
        '        val librarySection = makeSection(\n'
        '            "Library",\n'
        '            "Browse your recent recordings, processed copies, and trimmed exports. '
        'Search by file name or folder, then open a WAV in Editor or create a separate processed copy."\n'
        '        )\n'
    )
    settings_decl = (
        lib_anchor
        + "\n"
        + "        // Hybrid Studio Settings destination (dedicated SettingsScreen.kt)\n"
        + "        val settingsSection = LinearLayout(this).apply {\n"
        + "            orientation = LinearLayout.VERTICAL\n"
        + "            addView(\n"
        + "                SettingsScreen.build(\n"
        + "                    this@MainActivity,\n"
        + "                    resources.displayMetrics.density,\n"
        + "                    onResetProcessingToNatural = { applyProcessingPreset(\"Natural\") },\n"
        + "                ),\n"
        + "                matchWidth(),\n"
        + "            )\n"
        + "        }\n"
    )
    if "val settingsSection" not in text:
        if lib_anchor not in text:
            raise SystemExit("librarySection makeSection anchor not found")
        text = text.replace(lib_anchor, settings_decl, 1)

    # 2) Initial GONE with other non-record sections
    init_anchor = (
        "        editSection.visibility = android.view.View.GONE\n"
        "        processingSection.visibility = android.view.View.GONE\n"
        "        librarySection.visibility = android.view.View.GONE\n"
        "        creatorSection.visibility = android.view.View.GONE\n"
    )
    init_new = (
        "        editSection.visibility = android.view.View.GONE\n"
        "        processingSection.visibility = android.view.View.GONE\n"
        "        librarySection.visibility = android.view.View.GONE\n"
        "        creatorSection.visibility = android.view.View.GONE\n"
        "        settingsSection.visibility = android.view.View.GONE\n"
    )
    if "settingsSection.visibility = android.view.View.GONE" not in text:
        if init_anchor not in text:
            raise SystemExit("initial section GONE anchor not found")
        text = text.replace(init_anchor, init_new, 1)

    # 3) root.addView for settingsSection
    add_anchor = (
        "        root.addView(librarySection, sectionLayoutParams())\n\n"
        "        val navBar = LinearLayout(this).apply {\n"
    )
    add_new = (
        "        root.addView(librarySection, sectionLayoutParams())\n"
        "        root.addView(settingsSection, sectionLayoutParams())\n\n"
        "        val navBar = LinearLayout(this).apply {\n"
    )
    if "root.addView(settingsSection" not in text:
        if add_anchor not in text:
            raise SystemExit("root.addView librarySection anchor not found")
        text = text.replace(add_anchor, add_new, 1)

    # 4) navButton click visibility
    vis_anchor = (
        "                    librarySection.visibility = if (target === librarySection) "
        "android.view.View.VISIBLE else android.view.View.GONE\n"
        "                    applyNavSelection(target)\n"
    )
    vis_new = (
        "                    librarySection.visibility = if (target === librarySection) "
        "android.view.View.VISIBLE else android.view.View.GONE\n"
        "                    settingsSection.visibility = if (target === settingsSection) "
        "android.view.View.VISIBLE else android.view.View.GONE\n"
        "                    applyNavSelection(target)\n"
    )
    if "settingsSection.visibility = if (target === settingsSection)" not in text:
        if vis_anchor not in text:
            raise SystemExit("nav visibility librarySection anchor not found")
        text = text.replace(vis_anchor, vis_new, 1)

    # 5) Bottom nav item
    nav_anchor = (
        '            navButton("Library", "▤", librarySection)\n'
        "        ).forEach { navBar.addView(it, LinearLayout.LayoutParams(0, ViewGroup."
    )
    # Keep the forEach line intact — only insert Settings before closing
    nav_old = '            navButton("Library", "▤", librarySection)\n        ).forEach'
    nav_new = (
        '            navButton("Library", "▤", librarySection),\n'
        '            navButton("Settings", "⚙", settingsSection)\n'
        "        ).forEach"
    )
    if 'navButton("Settings"' not in text:
        if nav_old not in text:
            raise SystemExit("navButton Library list anchor not found")
        text = text.replace(nav_old, nav_new, 1)

    # 6) When other code paths force GONE on non-editor sections, also hide settings
    # Only after librarySection.visibility = GONE in multi-line blocks that hide all workspaces
    # Optional: append after each "librarySection.visibility = android.view.View.GONE\n"
    # that is NOT the nav if-expression — careful. Skip bulk replace to avoid double lines in nav.

    if text == original:
        print("Android Settings wiring: no changes applied (unexpected)")
        return
    MAIN.write_text(text, encoding="utf-8")
    print("Android Settings real UI wired into MainActivity navigation")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "android"
    if target in ("all", "android"):
        ensure_android()
    print("ci_apply_android_settings_real_ui:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
