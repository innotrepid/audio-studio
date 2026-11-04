#!/usr/bin/env python3
"""Apply Creator teleprompter Record-flow fixes in CI (GitHub-only friendly)."""
from __future__ import annotations

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


def ensure_android_mainactivity():
    ma = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
    text = ma.read_text(encoding="utf-8")
    if "studioTeleprompterHeight" in text and "creatorTeleprompterPanel" in text:
        print("MainActivity already has teleprompter layout changes")
        return

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

    ma.write_text(text, encoding="utf-8")
    text = ma.read_text(encoding="utf-8")
    if "studioTeleprompterHeight" not in text:
        raise SystemExit("studioTeleprompterHeight still missing after transform")
    print("Wrote Android MainActivity teleprompter updates")


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
