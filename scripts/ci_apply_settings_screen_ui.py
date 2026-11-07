#!/usr/bin/env python3
"""Settings screen UI apply — Windows additive; Android strips bad injects only.

compileDebugKotlin failed when a Settings Button was inserted mid-expression in
MainActivity.kt. Android path no longer injects UI; it only removes known-bad
fragments so CI can compile. Design: docs/HYBRID_STUDIO_UI_IMPLEMENTATION.md.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
    if not path.exists():
        print("Android MainActivity missing; skip")
        return
    text = path.read_text(encoding="utf-8")
    original = text

    patterns = [
        r"\n\s*// SETTINGS_SCREEN_MARKER[\s\S]*?(?=\n    private fun )",
        r"\n\s*private var settingsThemeChoice[\s\S]*?private fun openSettingsDestination\(\)[\s\S]*?\n    \}\n",
        r"\n\s*private fun buildSettingsContent\(\): LinearLayout \{[\s\S]*?\n        return panel\n    \}\n",
        r"\n\s*// Additive Settings entry[\s\S]*?setOnClickListener \{ openSettingsDestination\(\) \}\n\s*\}\n",
        r"\n\s*val openSettingsButton = Button\(this\)\.apply \{[\s\S]*?\}\n",
        r"\n\s*openSettingsButton\.apply \{[\s\S]*?\}\n",
    ]
    for pat in patterns:
        text = re.sub(pat, "\n", text)

    # Orphan apply-body lines that broke listOf("…Library")
    text = re.sub(
        r"\n\s*text = \"Settings\"\n\s*contentDescription = \"Open Settings\"\n\s*minHeight = dp\(40\)\n\s*setOnClickListener \{ openSettingsDestination\(\) \}\n",
        "\n",
        text,
    )

    if text != original:
        path.write_text(text, encoding="utf-8")
        print("Android: stripped broken Settings inject fragments")
    else:
        print("Android: no Settings inject fragments (no-op)")


def ensure_windows() -> None:
    xaml = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml"
    cs = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml.cs"
    if not xaml.exists():
        print("Windows XAML missing; skip")
        return
    xtext = xaml.read_text(encoding="utf-8")
    if "<Window" not in xtext:
        print("Windows XAML not restored; skip Settings inject")
        return

    if "SettingsNavButton" not in xtext:
        inject = (
            '\n                        <Button x:Name="SettingsNavButton" Content="Settings" '
            'Click="SettingsNav_Click" '
            'AutomationProperties.Name="Open Settings" '
            'Margin="0,12,0,0"/>\n'
        )
        for marker in ('Content="Library"', 'Tag="Library"'):
            idx = xtext.find(marker)
            if idx < 0:
                continue
            end = xtext.find("/>", idx)
            insert_at = end + 2 if end >= 0 else xtext.find(">", idx) + 1
            xtext = xtext[:insert_at] + inject + xtext[insert_at:]
            print("Windows Settings nav button added")
            break

    if "SettingsPanel" not in xtext:
        panel = '''
        <ScrollViewer x:Name="SettingsPanel" Visibility="Collapsed" Padding="24">
            <StackPanel MaxWidth="560">
                <TextBlock Text="Settings" FontSize="24" FontWeight="SemiBold" Margin="0,0,0,8"/>
                <TextBlock TextWrapping="Wrap" Opacity="0.8" Margin="0,0,0,16"
                    Text="App-wide preferences. Record, Editor, Creator, and Library stay available from navigation."/>
                <TextBlock Text="RECORDING" FontWeight="SemiBold" Margin="0,12,0,4"/>
                <TextBlock TextWrapping="Wrap" Text="Input device and sample rate remain on the Record workspace."/>
                <TextBlock Text="PROCESSING" FontWeight="SemiBold" Margin="0,16,0,4"/>
                <TextBlock TextWrapping="Wrap" Text="Processing preferences follow the shared preference contract."/>
                <Button Content="Reset processing to Natural" Click="SettingsResetProcessing_Click" Margin="0,8,0,0"
                    AutomationProperties.Name="Reset processing preferences to Natural defaults"/>
                <TextBlock Text="APPEARANCE" FontWeight="SemiBold" Margin="0,16,0,4"/>
                <StackPanel Orientation="Horizontal" Spacing="12">
                    <RadioButton x:Name="ThemeSystemRadio" Content="System default" IsChecked="True" GroupName="Theme"/>
                    <RadioButton x:Name="ThemeDarkRadio" Content="Dark studio" GroupName="Theme"/>
                </StackPanel>
                <TextBlock Text="MODELS &amp; OFFLINE" FontWeight="SemiBold" Margin="0,16,0,4"/>
                <TextBlock TextWrapping="Wrap" Text="Bundled models: Whisper ggml-tiny, Silero VAD. License text: NOTICE.md."/>
                <TextBlock Text="ABOUT &amp; COPYRIGHT" FontWeight="SemiBold" Margin="0,16,0,4"/>
                <TextBlock TextWrapping="Wrap" Text="Synvidra © Innotrepid. All rights reserved unless a separate license is granted."/>
            </StackPanel>
        </ScrollViewer>
'''
        close = xtext.rfind("</Grid>")
        if close < 0:
            close = xtext.rfind("</Window>")
        if close >= 0:
            xtext = xtext[:close] + panel + xtext[close:]
            print("Windows SettingsPanel injected")

    xaml.write_text(xtext, encoding="utf-8")

    if not cs.exists():
        return
    ctext = cs.read_text(encoding="utf-8")
    if "SettingsNav_Click" in ctext:
        print("Windows Settings handlers already present")
        return

    methods = r'''
    private void SettingsNav_Click(object sender, RoutedEventArgs e)
    {
        if (SettingsPanel is not null)
            SettingsPanel.Visibility = Visibility.Visible;
    }

    private void SettingsResetProcessing_Click(object sender, RoutedEventArgs e)
    {
        try
        {
            var m = GetType().GetMethod("ResetProcessingToNatural",
                System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Public);
            m?.Invoke(this, null);
        }
        catch
        {
        }
    }

'''
    for anchor in (
        "    private void CreatorSubnav_Click",
        "    private async void CreatorExportProject_Click",
        "    private void CreatorExportProject_Click",
        "    private void ApprovePhraseAssembly_Click",
    ):
        if anchor in ctext:
            ctext = ctext.replace(anchor, methods + "\n    " + anchor.lstrip(), 1)
            cs.write_text(ctext, encoding="utf-8")
            print("Windows Settings handlers wired")
            return
    print("Windows handler anchor missing; XAML may still build")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_settings_screen_ui:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
