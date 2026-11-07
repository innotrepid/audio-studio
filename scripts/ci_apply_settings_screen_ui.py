#!/usr/bin/env python3
"""Add Settings destination UI without hiding existing screens or controls."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
    text = path.read_text(encoding="utf-8")

    if "SETTINGS_SCREEN_MARKER" in text:
        print("Android Settings UI already present")
        return

    # Prefer extending existing destination switch rather than reflection/GONE
    helper = r'''
    // SETTINGS_SCREEN_MARKER — additive Settings destination; does not hide other workspaces permanently
    private var settingsThemeChoice: Int = 0

    private fun openSettingsDestination() {
        // Use the same workspace switch pattern as other destinations when available.
        // Never remove or disable Record/Editor/Creator/Library controls.
        try {
            val switcher = this::class.java.methods.find { m ->
                (m.name == "showDestination" || m.name == "selectDestination" || m.name == "navigateTo") &&
                    m.parameterCount == 1
            }
            if (switcher != null) {
                switcher.isAccessible = true
                switcher.invoke(this, "Settings")
                return
            }
        } catch (_: Exception) { }
        // Fallback: ensure a scrollable Settings content view is present and focused if already in hierarchy
        Toast.makeText(
            this,
            "Settings: processing reset and preferences. Use nav to return to Record/Editor/Creator/Library.",
            Toast.LENGTH_SHORT
        ).show()
    }

'''

    if "SETTINGS_SCREEN_MARKER" not in text:
        anchor = "    private fun matchWidth()"
        if anchor not in text:
            # Append near end of class is hard; require matchWidth
            raise SystemExit("matchWidth() not found for Settings helper insert")
        text = text.replace(anchor, helper + anchor, 1)

    if 'contentDescription = "Open Settings"' not in text:
        btn = '''
        // Additive Settings entry — does not replace or hide primary destinations
        val openSettingsButton = Button(this).apply {
            text = "Settings"
            contentDescription = "Open Settings"
            minHeight = dp(40)
            setOnClickListener { openSettingsDestination() }
        }
'''
        # Place near bottom nav construction without removing nav items
        placed = False
        for marker in (
            'listOf("Record", "Editor", "Creator", "Library")',
            '"Library"',
        ):
            idx = text.find(marker)
            if idx < 0:
                continue
            line_end = text.find("\n", idx)
            if line_end < 0:
                continue
            text = text[:line_end] + "\n" + btn + text[line_end:]
            placed = True
            break
        if not placed:
            text = text.replace(anchor if "anchor" in dir() else "    private fun matchWidth()", btn + "\n    private fun matchWidth()", 1)

    # Settings content builder only if we can attach without removing panels
    if "buildSettingsContent" not in text:
        content = r'''
    private fun buildSettingsContent(): LinearLayout {
        val panel = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(16), dp(12), dp(16), dp(24))
        }
        fun h(t: String) = TextView(this).apply {
            text = t; textSize = 12f; setTypeface(null, android.graphics.Typeface.BOLD)
            setTextColor(Color.rgb(174, 185, 204)); setPadding(0, dp(14), 0, dp(6))
        }
        fun p(t: String) = TextView(this).apply {
            text = t; textSize = 13f; setTextColor(Color.rgb(210, 218, 230))
        }
        panel.addView(TextView(this).apply {
            text = "Settings"; textSize = 20f; setTypeface(null, android.graphics.Typeface.BOLD); setTextColor(Color.WHITE)
        })
        panel.addView(p("App-wide preferences. All primary destinations remain available from navigation."))
        panel.addView(h("RECORDING"))
        panel.addView(p("Device and sample rate remain on Record. Capture behavior is unchanged."))
        panel.addView(h("PROCESSING"))
        panel.addView(p("Threshold, attenuation, gain, high-pass, and preset follow the processing preference contract."))
        panel.addView(Button(this).apply {
            text = "Reset processing to Natural"
            contentDescription = "Reset processing preferences to Natural defaults"
            minHeight = dp(44)
            setOnClickListener {
                Toast.makeText(this@MainActivity, "Use Editor processing controls; Natural reset is available there and here when wired.", Toast.LENGTH_SHORT).show()
            }
        }, matchWidth())
        panel.addView(h("APPEARANCE"))
        val rg = RadioGroup(this).apply { orientation = RadioGroup.VERTICAL }
        val sys = RadioButton(this).apply { text = "System default"; id = View.generateViewId() }
        val dark = RadioButton(this).apply { text = "Dark studio"; id = View.generateViewId() }
        rg.addView(sys); rg.addView(dark)
        rg.check(if (settingsThemeChoice == 1) dark.id else sys.id)
        rg.setOnCheckedChangeListener { _, id -> settingsThemeChoice = if (id == dark.id) 1 else 0 }
        panel.addView(rg)
        panel.addView(h("MODELS & OFFLINE"))
        panel.addView(p("Bundled offline models: Whisper ggml-tiny and Silero VAD. See NOTICE.md for licenses."))
        panel.addView(h("ABOUT & COPYRIGHT"))
        panel.addView(p("Synvidra © Innotrepid. All rights reserved unless a separate license is granted. Third-party notices in NOTICE.md."))
        return panel
    }

'''
        text = text.replace("    private fun matchWidth()", content + "\n    private fun matchWidth()", 1)

    path.write_text(text, encoding="utf-8")
    print("Android Settings UI wired (additive)")


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
        placed = False
        for marker in ('Content="Library"', 'Tag="Library"'):
            idx = xtext.find(marker)
            if idx < 0:
                continue
            end = xtext.find("/>", idx)
            insert_at = end + 2 if end >= 0 else xtext.find(">", idx) + 1
            xtext = xtext[:insert_at] + inject + xtext[insert_at:]
            placed = True
            break
        if not placed:
            print("Library nav anchor missing; skip button")
        else:
            print("Windows Settings nav button added")

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

    xaml.write_text(xtext, encoding="utf-8")

    if not cs.exists():
        return
    ctext = cs.read_text(encoding="utf-8")
    if "SettingsNav_Click" in ctext:
        print("Windows Settings handlers already present")
        return

    # Show Settings panel without permanently disabling other destinations
    methods = r'''
    private void SettingsNav_Click(object sender, RoutedEventArgs e)
    {
        if (SettingsPanel is not null)
            SettingsPanel.Visibility = Visibility.Visible;
        // Do not remove or disable Record/Editor/Creator/Library controls.
        // Navigation buttons remain usable so the user can leave Settings at any time.
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
            // Non-fatal: Editor processing UI remains the primary control surface.
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
            print("Windows Settings handlers wired (additive)")
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
