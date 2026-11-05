#!/usr/bin/env python3
"""Apply Creator teleprompter Record-flow fixes and preference persistence."""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def run(cmd):
    print("+", " ".join(cmd), flush=True)
    subprocess.check_call(cmd, cwd=ROOT)


def ensure_windows_xaml():
    xaml = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml"
    size = xaml.stat().st_size if xaml.exists() else 0
    print(f"MainWindow.xaml size={size}")
    if size < 5000:
        run(["git", "fetch", "--depth=1", "origin", "Redesign,-Teleprompter,-captions-&-more"])
        blob = subprocess.check_output(
            ["git", "show", "FETCH_HEAD:platforms/windows/AudioStudio/MainWindow.xaml"],
            cwd=ROOT,
        )
        xaml.write_bytes(blob)
        print(f"Restored XAML to {xaml.stat().st_size} bytes")

    text = xaml.read_text(encoding="utf-8")
    needed = [
        "RecoverRecordingsButton",
        "RecoveryStatusText",
        "InputLevelBar",
        "InputLevelText",
    ]
    if any(f'x:Name="{n}"' not in text for n in needed):
        snippet = (
            '\n                        <ProgressBar x:Name="InputLevelBar" Minimum="0" Maximum="100" Value="0" Height="6" Visibility="Collapsed"/>\n'
            '                        <TextBlock x:Name="InputLevelText" Text="" Visibility="Collapsed"/>\n'
            '                        <Button x:Name="RecoverRecordingsButton" Content="Check for interrupted recordings..." Click="RecoverRecordings_Click" Visibility="Collapsed"/>\n'
            '                        <TextBlock x:Name="RecoveryStatusText" Text="" Visibility="Collapsed"/>\n'
        )
        marker = 'x:Name="OpenLatestTakeFromRecordButton"'
        if marker in text:
            line_end = text.find("/>", text.find(marker)) + 2
            text = text[:line_end] + snippet + text[line_end:]
        else:
            insert_at = text.find("</Grid>\n            </Border>")
            if insert_at < 0:
                raise SystemExit("Cannot find insertion point for recovery/level controls")
            text = text[:insert_at] + snippet + text[insert_at:]
        xaml.write_text(text, encoding="utf-8")
        print("Injected missing recovery/level controls into XAML")

    text = xaml.read_text(encoding="utf-8")
    for name in needed:
        if f'x:Name="{name}"' not in text:
            raise SystemExit(f"{name} still missing from XAML")
    print("Windows XAML named controls OK")


def wire_windows_teleprompter_prefs(text: str) -> str:
    if "PersistTeleprompterPreferences()" in text and "TeleprompterPreferences.Load()" in text:
        print("Windows teleprompter prefs already wired")
        return text

    for field in (
        "CreatorTeleprompterSpeedSlider",
        "CreatorTeleprompterFontSizeSlider",
        "CreatorTeleprompterLineSpacingSlider",
    ):
        old = (
            f"        if ({field} != null)\n"
            f"            {field}.ValueChanged += (_, _) => SyncStudioTeleprompterFromCreator();"
        )
        new = (
            f"        if ({field} != null)\n"
            f"            {field}.ValueChanged += (_, _) =>\n"
            f"            {{\n"
            f"                SyncStudioTeleprompterFromCreator();\n"
            f"                PersistTeleprompterPreferences();\n"
            f"            }};"
        )
        if old in text:
            text = text.replace(old, new, 1)

    old_cd = (
        "        if (CreatorTeleprompterCountdownComboBox != null)\n"
        "            CreatorTeleprompterCountdownComboBox.SelectionChanged += (_, _) => SyncStudioTeleprompterFromCreator();"
    )
    new_cd = (
        "        if (CreatorTeleprompterCountdownComboBox != null)\n"
        "            CreatorTeleprompterCountdownComboBox.SelectionChanged += (_, _) =>\n"
        "            {\n"
        "                SyncStudioTeleprompterFromCreator();\n"
        "                PersistTeleprompterPreferences();\n"
        "            };\n"
        "\n"
        "        try\n"
        "        {\n"
        "            var tp = TeleprompterPreferences.Load();\n"
        "            if (CreatorTeleprompterSpeedSlider != null)\n"
        "                CreatorTeleprompterSpeedSlider.Value = tp.Speed;\n"
        "            if (CreatorTeleprompterFontSizeSlider != null)\n"
        "                CreatorTeleprompterFontSizeSlider.Value = tp.FontSize;\n"
        "            if (CreatorTeleprompterLineSpacingSlider != null)\n"
        "                CreatorTeleprompterLineSpacingSlider.Value = tp.LineSpacing;\n"
        "            if (CreatorTeleprompterCountdownComboBox != null\n"
        "                && tp.CountdownIndex >= 0\n"
        "                && tp.CountdownIndex < CreatorTeleprompterCountdownComboBox.Items.Count)\n"
        "                CreatorTeleprompterCountdownComboBox.SelectedIndex = tp.CountdownIndex;\n"
        "            SyncStudioTeleprompterFromCreator();\n"
        "        }\n"
        "        catch\n"
        "        {\n"
        "            // LocalSettings unavailable in some hosts; keep XAML defaults.\n"
        "        }\n"
    )
    if old_cd in text:
        text = text.replace(old_cd, new_cd, 1)
        print("Wired Windows teleprompter prefs load/save handlers")

    if "void PersistTeleprompterPreferences()" not in text:
        method = (
            "    private void PersistTeleprompterPreferences()\n"
            "    {\n"
            "        try\n"
            "        {\n"
            "            TeleprompterPreferences.Save(\n"
            "                CreatorTeleprompterSpeedSlider?.Value ?? 42,\n"
            "                CreatorTeleprompterFontSizeSlider?.Value ?? 28,\n"
            "                CreatorTeleprompterLineSpacingSlider?.Value ?? 1.5,\n"
            "                CreatorTeleprompterCountdownComboBox?.SelectedIndex ?? 0);\n"
            "        }\n"
            "        catch\n"
            "        {\n"
            "            // Ignore preference write failures; UI remains usable.\n"
            "        }\n"
            "    }\n"
            "\n"
        )
        for anchor in (
            "    private void SyncStudioTeleprompterFromCreator()",
            "    private void RefreshCreatorProductionMarkers()",
        ):
            if anchor in text:
                text = text.replace(anchor, method + anchor, 1)
                print("Injected PersistTeleprompterPreferences method")
                break

    return text


def ensure_windows_cs():
    cs = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml.cs"
    text = cs.read_text(encoding="utf-8")
    if "private void SyncStudioTeleprompterFromCreator()" in text:
        print("SyncStudioTeleprompterFromCreator already present")
        text2 = wire_windows_teleprompter_prefs(text)
        if text2 != text:
            cs.write_text(text2, encoding="utf-8")
            print("Wrote Windows teleprompter prefs wiring")
        return

    method = (
        "    private void SyncStudioTeleprompterFromCreator()\n"
        "    {\n"
        "        var scriptText = CreatorScriptBodyBox?.Text;\n"
        "        if (string.IsNullOrWhiteSpace(scriptText))\n"
        "        {\n"
        "            scriptText = SelectedCreatorScript?.Sections?\n"
        "                .OrderBy(section => section.Order)\n"
        "                .Select(section => section.Text)\n"
        "                .FirstOrDefault(text => !string.IsNullOrWhiteSpace(text));\n"
        "        }\n"
        "        scriptText = string.IsNullOrWhiteSpace(scriptText)\n"
        '            ? "Write a script under Creator Scripts. It appears here and on Record automatically."\n'
        "            : scriptText;\n"
        "\n"
        "        if (CreatorTeleprompterPreviewText != null)\n"
        "            CreatorTeleprompterPreviewText.Text = scriptText;\n"
        "        if (TeleprompterScriptText != null)\n"
        "            TeleprompterScriptText.Text = scriptText;\n"
        "\n"
        "        if (CreatorTeleprompterSpeedSlider != null && TeleprompterSpeedSlider != null)\n"
        "            TeleprompterSpeedSlider.Value = CreatorTeleprompterSpeedSlider.Value;\n"
        "        if (CreatorTeleprompterFontSizeSlider != null && TeleprompterFontSizeSlider != null)\n"
        "            TeleprompterFontSizeSlider.Value = CreatorTeleprompterFontSizeSlider.Value;\n"
        "        if (CreatorTeleprompterLineSpacingSlider != null && TeleprompterLineSpacingSlider != null)\n"
        "            TeleprompterLineSpacingSlider.Value = CreatorTeleprompterLineSpacingSlider.Value;\n"
        "        if (CreatorTeleprompterCountdownComboBox != null && TeleprompterCountdownComboBox != null\n"
        "            && CreatorTeleprompterCountdownComboBox.SelectedIndex >= 0)\n"
        "            TeleprompterCountdownComboBox.SelectedIndex = CreatorTeleprompterCountdownComboBox.SelectedIndex;\n"
        "\n"
        "        ApplyStudioTeleprompterTypography();\n"
        "        if (CreatorTeleprompterStatusText != null)\n"
        '            CreatorTeleprompterStatusText.Text = "Mirrored to Record studio teleprompter.";\n'
        "    }\n"
        "\n"
        "    private void ApplyStudioTeleprompterTypography()\n"
        "    {\n"
        "        if (TeleprompterScriptText == null || TeleprompterFontSizeSlider == null || TeleprompterLineSpacingSlider == null)\n"
        "            return;\n"
        "        var size = TeleprompterFontSizeSlider.Value;\n"
        "        TeleprompterScriptText.FontSize = size;\n"
        "        TeleprompterScriptText.LineHeight = size * TeleprompterLineSpacingSlider.Value;\n"
        "        if (CreatorTeleprompterPreviewText != null)\n"
        "        {\n"
        "            CreatorTeleprompterPreviewText.FontSize = size;\n"
        "            CreatorTeleprompterPreviewText.LineHeight = size * TeleprompterLineSpacingSlider.Value;\n"
        "        }\n"
        "        if (CreatorTeleprompterSpeedText != null && CreatorTeleprompterSpeedSlider != null)\n"
        '            CreatorTeleprompterSpeedText.Text = $"Scroll speed \u00b7 {CreatorTeleprompterSpeedSlider.Value:0} px/s";\n'
        "        if (CreatorTeleprompterFontSizeText != null)\n"
        '            CreatorTeleprompterFontSizeText.Text = $"Text size \u00b7 {size:0} px";\n'
        "        if (CreatorTeleprompterLineSpacingText != null && CreatorTeleprompterLineSpacingSlider != null)\n"
        '            CreatorTeleprompterLineSpacingText.Text = $"Line spacing \u00b7 {CreatorTeleprompterLineSpacingSlider.Value:0.0}x";\n'
        "    }\n"
        "\n"
    )
    anchor = "    private void RefreshCreatorProductionMarkers()"
    if anchor not in text:
        raise SystemExit("Cannot find RefreshCreatorProductionMarkers anchor")
    text = text.replace(anchor, method + anchor, 1)
    print("Injected SyncStudioTeleprompterFromCreator")
    text = wire_windows_teleprompter_prefs(text)
    cs.write_text(text, encoding="utf-8")
    print("Wrote Windows MainWindow.xaml.cs teleprompter updates")


def _strip_kotlin_comments(s: str) -> str:
    s = re.sub(r"//.*?$", "", s, flags=re.M)
    s = re.sub(r"/\*.*?\*/", "", s, flags=re.S)
    return s


def fix_android_forward_refs(text: str) -> str:
    old_block = (
        "                    if (teleprompterSelected) {\n"
        "                        val text = creatorBodyInput.text?.toString().orEmpty().ifBlank {\n"
        '                            "Write a script under Scripts to preview it here."\n'
        "                        }\n"
        "                        creatorTeleprompterPreview.text = text\n"
        "                        teleprompterScriptView?.text = text\n"
        "                    }"
    )
    new_block = (
        "                    if (teleprompterSelected) {\n"
        "                        // Script mirror is maintained by the text watcher after views exist.\n"
        "                        teleprompterScriptView?.let { view ->\n"
        "                            val mirrored = view.text?.toString().orEmpty()\n"
        "                            if (mirrored.isBlank()) {\n"
        '                                view.text = "Write a script under Scripts to preview it here."\n'
        "                            }\n"
        "                        }\n"
        "                    }"
    )
    if old_block in text:
        text = text.replace(old_block, new_block, 1)
        print("Fixed teleprompterSelected forward refs")

    old_scroll = (
        "                    screenScroll.setOnTouchListener { _, _ -> target === recordSection }\n"
    )
    if old_scroll in text:
        text = text.replace(
            old_scroll,
            "                    recordScrollLocked = target === recordSection\n",
            1,
        )
        print("Replaced early screenScroll touch lock with recordScrollLocked flag")

    if "var recordScrollLocked" not in text and "recordScrollLocked =" in text:
        inserted = False
        for anchor in (
            "        val navItems",
            "        fun navButton",
            '        listOf(\n            navButton("Record"',
        ):
            if anchor in text:
                text = text.replace(anchor, "        var recordScrollLocked = false\n" + anchor, 1)
                inserted = True
                print("Declared recordScrollLocked")
                break
        if not inserted:
            raise SystemExit("Could not declare recordScrollLocked near nav setup")

    old_sv = (
        "        val screenScroll = ScrollView(this).apply {\n"
        "            isFillViewport = true\n"
        "            clipToPadding = false\n"
        "            setBackgroundColor(Color.rgb(16, 18, 24))\n"
        "            addView(root, ViewGroup.LayoutParams(\n"
        "                ViewGroup.LayoutParams.MATCH_PARENT,\n"
        "                ViewGroup.LayoutParams.WRAP_CONTENT\n"
        "            ))\n"
        "        }"
    )
    new_sv = (
        "        val screenScroll = ScrollView(this).apply {\n"
        "            isFillViewport = true\n"
        "            clipToPadding = false\n"
        "            setBackgroundColor(Color.rgb(16, 18, 24))\n"
        "            setOnTouchListener { _, _ -> recordScrollLocked }\n"
        "            addView(root, ViewGroup.LayoutParams(\n"
        "                ViewGroup.LayoutParams.MATCH_PARENT,\n"
        "                ViewGroup.LayoutParams.WRAP_CONTENT\n"
        "            ))\n"
        "        }"
    )
    if old_sv in text and "setOnTouchListener { _, _ -> recordScrollLocked }" not in text:
        text = text.replace(old_sv, new_sv, 1)
        print("Wired screenScroll touch listener after declaration")

    code = _strip_kotlin_comments(text)
    if "val creatorBodyInput" in code:
        before, _, _ = code.partition("val creatorBodyInput")
        if re.search(r"\bcreatorBodyInput\b", before):
            raise SystemExit("creatorBodyInput still referenced before declaration")
    if "val screenScroll" in code:
        before, _, _ = code.partition("val screenScroll")
        if re.search(r"\bscreenScroll\b", before):
            raise SystemExit("screenScroll still referenced before declaration")

    return text


def wire_android_teleprompter_prefs(text: str) -> str:
    if "TeleprompterPreferences.load" in text:
        print("Android teleprompter prefs already wired")
        return text

    old_cd = (
        '                listOf("3 second countdown", "5 second countdown", "10 second countdown")\n'
    )
    new_cd = (
        '                listOf("3 second countdown", "5 second countdown", "10 second countdown", "No pre-roll")\n'
    )
    if old_cd in text:
        text = text.replace(old_cd, new_cd, 1)

    load_snippet = (
        "        val teleprompterPrefs = TeleprompterPreferences.load(this)\n"
        "        teleprompterSpeed.progress = teleprompterPrefs.speedProgress\n"
        "        teleprompterFontSize.progress = teleprompterPrefs.fontProgress\n"
        "        teleprompterLineSpacing.progress = teleprompterPrefs.spacingProgress\n"
        "        teleprompterCountdown.setSelection(teleprompterPrefs.countdownIndex.coerceIn(0, teleprompterCountdown.adapter.count - 1))\n"
        "        teleprompterSpeedPxPerSecond = TeleprompterPreferences.speedPxPerSecond(teleprompterSpeed.progress)\n"
        '        teleprompterSpeedLabel.text = "Scroll speed \u00b7 ${teleprompterSpeedPxPerSecond.toInt()} px/s"\n'
        '        teleprompterFontSizeLabel.text = "Text size \u00b7 ${TeleprompterPreferences.fontSizeSp(teleprompterFontSize.progress)} sp"\n'
        "        teleprompterScriptView?.textSize = TeleprompterPreferences.fontSizeSp(teleprompterFontSize.progress).toFloat()\n"
        "        teleprompterPendingCountdownSeconds = TeleprompterPreferences.countdownSeconds(teleprompterCountdown.selectedItemPosition)\n"
    )

    save_stop = (
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

    unit_stop = "                override fun onStopTrackingTouch(seekBar: SeekBar?) = Unit\n"

    text = text.replace(
        unit_stop + "            })\n        }\n        val teleprompterFontSizeLabel",
        save_stop + "            })\n        }\n        val teleprompterFontSizeLabel",
        1,
    )
    text = text.replace(
        unit_stop + "            })\n        }\n        val teleprompterLineSpacingLabel",
        save_stop + "            })\n        }\n        val teleprompterLineSpacingLabel",
        1,
    )
    text = text.replace(
        unit_stop + "            })\n        }\n        val teleprompterControls",
        save_stop + "            })\n        }\n" + load_snippet + "        val teleprompterControls",
        1,
    )

    if "creatorTeleprompterPanel.addView(teleprompterCountdown" not in text:
        text = text.replace(
            "        creatorTeleprompterPanel.addView(teleprompterLineSpacing, matchWidth())\n",
            "        creatorTeleprompterPanel.addView(teleprompterLineSpacing, matchWidth())\n"
            "        creatorTeleprompterPanel.addView(TextView(this).apply {\n"
            '            text = "DEFAULT PRE-ROLL"\n'
            "            textSize = 11f\n"
            "            setTextColor(Color.rgb(174, 185, 204))\n"
            "            setPadding(0, dp(8), 0, dp(4))\n"
            "        }, matchWidth())\n"
            "        creatorTeleprompterPanel.addView(teleprompterCountdown, matchWidth())\n"
            "        creatorTeleprompterPanel.addView(TextView(this).apply {\n"
            '            text = "Settings apply to Record studio teleprompter and survive app restart."\n'
            "            textSize = 12f\n"
            "            setTextColor(Color.rgb(174, 185, 204))\n"
            "            setPadding(0, dp(6), 0, dp(4))\n"
            "        }, matchWidth())\n",
            1,
        )
        text = text.replace(
            "        recordSection.addView(teleprompterCountdown, matchWidth())\n",
            "        // Pre-roll spinner lives under Creator Teleprompter (settings owner).\n",
            1,
        )
        print("Moved countdown control to Creator Teleprompter panel")

    if "teleprompterCountdown.onItemSelectedListener" not in text:
        listener = (
            "        teleprompterCountdown.onItemSelectedListener = object : android.widget.AdapterView.OnItemSelectedListener {\n"
            "            override fun onItemSelected(parent: android.widget.AdapterView<*>?, view: android.view.View?, position: Int, id: Long) {\n"
            "                teleprompterPendingCountdownSeconds = TeleprompterPreferences.countdownSeconds(position)\n"
            "                TeleprompterPreferences.save(\n"
            "                    this@MainActivity,\n"
            "                    teleprompterSpeed.progress,\n"
            "                    teleprompterFontSize.progress,\n"
            "                    teleprompterLineSpacing.progress,\n"
            "                    position,\n"
            "                )\n"
            "            }\n"
            "            override fun onNothingSelected(parent: android.widget.AdapterView<*>?) = Unit\n"
            "        }\n"
        )
        mark = "        creatorBodyInput.addTextChangedListener"
        if mark in text:
            text = text.replace(mark, listener + "\n        " + mark, 1)
            print("Wired countdown spinner persist listener")

    print("Android teleprompter prefs wiring applied")
    return text


def ensure_android_mainactivity():
    ma = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
    text = ma.read_text(encoding="utf-8")
    original = text
    text = fix_android_forward_refs(text)
    text = wire_android_teleprompter_prefs(text)
    if text != original:
        ma.write_text(text, encoding="utf-8")
        print("Wrote Android MainActivity fixes")
    else:
        print("MainActivity already up to date")


def main():
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "windows"):
        ensure_windows_xaml()
        ensure_windows_cs()
    if target in ("all", "android"):
        ensure_android_mainactivity()
    print("ci_apply_teleprompter_ui:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
