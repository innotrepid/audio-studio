#!/usr/bin/env python3
"""Apply Creator teleprompter Record-flow fixes in CI (GitHub-only friendly)."""
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


def ensure_windows_cs():
    cs = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml.cs"
    text = cs.read_text(encoding="utf-8")
    if "private void SyncStudioTeleprompterFromCreator()" in text:
        print("SyncStudioTeleprompterFromCreator already present")
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
        '            CreatorTeleprompterSpeedText.Text = $"Scroll speed · {CreatorTeleprompterSpeedSlider.Value:0} px/s";\n'
        "        if (CreatorTeleprompterFontSizeText != null)\n"
        '            CreatorTeleprompterFontSizeText.Text = $"Text size · {size:0} px";\n'
        "        if (CreatorTeleprompterLineSpacingText != null && CreatorTeleprompterLineSpacingSlider != null)\n"
        '            CreatorTeleprompterLineSpacingText.Text = $"Line spacing · {CreatorTeleprompterLineSpacingSlider.Value:0.0}x";\n'
        "    }\n"
        "\n"
    )
    anchor = "    private void RefreshCreatorProductionMarkers()"
    if anchor not in text:
        raise SystemExit("Cannot find RefreshCreatorProductionMarkers anchor")
    cs.write_text(text.replace(anchor, method + anchor, 1), encoding="utf-8")
    print("Injected SyncStudioTeleprompterFromCreator")


def _strip_kotlin_comments(s: str) -> str:
    s = re.sub(r"//.*?$", "", s, flags=re.M)
    s = re.sub(r"/\*.*?\*/", "", s, flags=re.S)
    return s


def fix_android_forward_refs(text: str) -> str:
    """Fix Kotlin forward-reference compile errors in MainActivity.onCreate."""
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
            "        listOf(\n            navButton(\"Record\"",
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
    if "val creatorTeleprompterPreview" in code:
        before, _, _ = code.partition("val creatorTeleprompterPreview")
        if re.search(r"\bcreatorTeleprompterPreview\b", before):
            raise SystemExit("creatorTeleprompterPreview still referenced before declaration")
    if "val screenScroll" in code:
        before, _, _ = code.partition("val screenScroll")
        if re.search(r"\bscreenScroll\b", before):
            raise SystemExit("screenScroll still referenced before declaration")

    return text


def ensure_android_mainactivity():
    ma = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
    text = ma.read_text(encoding="utf-8")
    original = text

    text = fix_android_forward_refs(text)

    if "studioTeleprompterHeight" not in text:
        print("Applying Android teleprompter layout transforms in Python")
        if "val creatorTeleprompterPanel" not in text:
            text = text.replace(
                "        val creatorScriptsPanel = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }\n",
                "        val creatorScriptsPanel = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }\n"
                "        val creatorTeleprompterPanel = LinearLayout(this).apply {\n"
                "            orientation = LinearLayout.VERTICAL\n"
                "            visibility = ViewGroup.GONE\n"
                "        }\n",
                1,
            )

        old_attach = (
            "        // Attach teleprompter views only after their local declarations are initialized.\n"
            "        recordSection.addView(teleprompterHeading, matchWidth())\n"
            "        recordSection.addView(teleprompterScroll, LinearLayout.LayoutParams(\n"
            "            ViewGroup.LayoutParams.MATCH_PARENT, dp(300)\n"
            "        ))\n"
            "        recordSection.addView(teleprompterCountdown, matchWidth())\n"
            "        recordSection.addView(teleprompterSpeedLabel, matchWidth())\n"
            "        recordSection.addView(teleprompterSpeed, matchWidth())\n"
            "        recordSection.addView(teleprompterFontSizeLabel, matchWidth())\n"
            "        recordSection.addView(teleprompterFontSize, matchWidth())\n"
            "        recordSection.addView(teleprompterLineSpacingLabel, matchWidth())\n"
            "        recordSection.addView(teleprompterLineSpacing, matchWidth())\n"
            "        recordSection.addView(teleprompterControls, matchWidth())\n"
            "        recordSection.addView(teleprompterCountdownStatus, matchWidth())\n"
        )
        new_attach = (
            "        // Studio teleprompter: maximize height; settings live under Creator Teleprompter.\n"
            '        teleprompterHeading.text = "STUDIO TELEPROMPTER"\n'
            "        val studioTeleprompterHeight = (resources.displayMetrics.heightPixels * 0.52f).toInt().coerceAtLeast(dp(280))\n"
            "        recordSection.addView(teleprompterHeading, matchWidth())\n"
            "        recordSection.addView(TextView(this).apply {\n"
            '            text = "Script and settings come from Creator Teleprompter."\n'
            "            textSize = 11f\n"
            "            setTextColor(Color.rgb(174, 185, 204))\n"
            "            setPadding(0, 0, 0, dp(6))\n"
            "        }, matchWidth())\n"
            "        recordSection.addView(teleprompterScroll, LinearLayout.LayoutParams(\n"
            "            ViewGroup.LayoutParams.MATCH_PARENT, studioTeleprompterHeight\n"
            "        ))\n"
            "        recordSection.addView(teleprompterCountdown, matchWidth())\n"
            '        teleprompterStart.text = "Pre-roll + record"\n'
            '        teleprompterPause.text = "Pause"\n'
            "        recordSection.addView(teleprompterControls, matchWidth())\n"
            "        recordSection.addView(teleprompterCountdownStatus, matchWidth())\n"
        )
        if old_attach in text:
            text = text.replace(old_attach, new_attach, 1)

        text = text.replace(
            "if (startTeleprompter) startTeleprompterScroll()",
            "startTeleprompterScroll()",
            1,
        )

        if "creatorWorkspaceContent.addView(creatorTeleprompterPanel" not in text:
            text = text.replace(
                "creatorWorkspaceContent.addView(creatorScriptsPanel, matchWidth())\n",
                "creatorWorkspaceContent.addView(creatorScriptsPanel, matchWidth())\n"
                "        creatorWorkspaceContent.addView(creatorTeleprompterPanel, matchWidth())\n",
                1,
            )

    if text != original:
        ma.write_text(text, encoding="utf-8")
        print("Wrote Android MainActivity fixes")
    else:
        print("MainActivity already compile-clean for teleprompter forward refs")


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
