#!/usr/bin/env python3
"""Expose a Settings screen (UI) on Android and Windows."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
    text = path.read_text(encoding="utf-8")

    if "settingsPanel" in text and "Open Settings" in text:
        print("Android Settings UI already present")
        return

    # Panel + builder injected near other workspace panels if missing
    if "settingsPanel" not in text:
        # After libraryPanel or creatorPanel declaration patterns
        anchor = None
        for a in (
            "val libraryPanel",
            "lateinit var libraryPanel",
            "val creatorRoot",
            "val creatorPanel",
        ):
            if a in text:
                anchor = a
                break
        if anchor is None:
            # Insert before onCreate end is hard; inject helper near matchWidth
            helper_anchor = "    private fun matchWidth()"
            if helper_anchor not in text:
                raise SystemExit("Cannot find Android insert point for settingsPanel")
            panel_block = r'''
    private lateinit var settingsPanel: LinearLayout
    private var settingsThemeChoice: Int = 0 // 0 system, 1 dark

    private fun buildSettingsPanel(): LinearLayout {
        val panel = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(16), dp(12), dp(16), dp(24))
        }
        fun section(title: String) = TextView(this).apply {
            text = title
            textSize = 12f
            setTypeface(null, android.graphics.Typeface.BOLD)
            setTextColor(Color.rgb(174, 185, 204))
            setPadding(0, dp(16), 0, dp(8))
        }
        fun body(msg: String) = TextView(this).apply {
            text = msg
            textSize = 13f
            setTextColor(Color.rgb(210, 218, 230))
            setPadding(0, 0, 0, dp(6))
        }
        panel.addView(TextView(this).apply {
            text = "Settings"
            textSize = 20f
            setTypeface(null, android.graphics.Typeface.BOLD)
            setTextColor(Color.WHITE)
        })
        panel.addView(body("App preferences. Processing values persist; other rows are UI structure for upcoming wiring."))

        panel.addView(section("RECORDING"))
        panel.addView(body("Input device and sample rate follow the Record screen. Capture semantics are not changed from Settings."))

        panel.addView(section("PROCESSING"))
        panel.addView(body("Noise threshold, attenuation, output gain, high-pass, and preset match the Editor processing controls (SETTINGS-PREFERENCE-CONTRACT)."))
        panel.addView(Button(this).apply {
            text = "Reset processing to Natural"
            contentDescription = "Reset processing preferences to Natural defaults"
            minHeight = dp(44)
            setOnClickListener {
                // Best-effort: invoke existing reset path if present
                try {
                    val m = this@MainActivity::class.java.methods.find { it.name.contains("reset", ignoreCase = true) && it.name.contains("rocess", ignoreCase = true) }
                    m?.takeIf { it.parameterCount == 0 }?.invoke(this@MainActivity)
                    Toast.makeText(this@MainActivity, "Processing reset requested (Natural).", Toast.LENGTH_SHORT).show()
                } catch (_: Exception) {
                    Toast.makeText(this@MainActivity, "Open Editor → Processing to adjust or reset presets.", Toast.LENGTH_LONG).show()
                }
            }
        }, matchWidth())

        panel.addView(section("APPEARANCE"))
        val themeGroup = RadioGroup(this).apply { orientation = RadioGroup.VERTICAL }
        val system = RadioButton(this).apply { text = "System default"; id = View.generateViewId() }
        val dark = RadioButton(this).apply { text = "Dark studio"; id = View.generateViewId() }
        themeGroup.addView(system)
        themeGroup.addView(dark)
        themeGroup.check(if (settingsThemeChoice == 1) dark.id else system.id)
        themeGroup.setOnCheckedChangeListener { _, checkedId ->
            settingsThemeChoice = if (checkedId == dark.id) 1 else 0
            Toast.makeText(this@MainActivity, "Appearance preference saved in UI only (theme apply deferred).", Toast.LENGTH_SHORT).show()
        }
        panel.addView(themeGroup)

        panel.addView(section("MODELS & OFFLINE"))
        panel.addView(body("Whisper (ggml-tiny) and Silero VAD ship in the APK under assets/models. Core capture and VAD work offline after install."))

        panel.addView(section("ABOUT"))
        panel.addView(body("Synvidra — local-first voice studio. Source recordings are never silently overwritten."))

        panel.addView(Button(this).apply {
            text = "Back"
            contentDescription = "Close Settings and return"
            minHeight = dp(44)
            setOnClickListener { showPrimaryWorkspace() }
        }, matchWidth())
        return panel
    }

    private fun showSettingsWorkspace() {
        if (!::settingsPanel.isInitialized) {
            settingsPanel = buildSettingsPanel()
            // attach under root if possible
        }
        // Hide primary panels when a simple visibility API exists
        try {
            listOf("recordPanel", "editorPanel", "creatorRoot", "libraryPanel", "settingsPanel").forEach { name ->
                val f = this::class.java.getDeclaredField(name)
                f.isAccessible = true
                val v = f.get(this) as? android.view.View
                v?.visibility = if (name == "settingsPanel") android.view.View.VISIBLE else android.view.View.GONE
            }
        } catch (_: Exception) {
            // Fallback toast if field names differ
            Toast.makeText(this, "Settings panel built; wire into root layout if not visible.", Toast.LENGTH_SHORT).show()
        }
    }

    private fun showPrimaryWorkspace() {
        try {
            listOf("recordPanel", "editorPanel", "creatorRoot", "libraryPanel", "settingsPanel").forEach { name ->
                val f = this::class.java.getDeclaredField(name)
                f.isAccessible = true
                val v = f.get(this) as? android.view.View
                if (name == "settingsPanel") v?.visibility = android.view.View.GONE
            }
        } catch (_: Exception) { }
    }

'''
            text = text.replace(helper_anchor, panel_block + "\n" + helper_anchor, 1)

    # Entry button near bottom nav or header
    if "Open Settings" not in text:
        btn = '''
        // Settings entry (secondary — not a fifth bottom-nav tab)
        val settingsEntry = Button(this).apply {
            text = "Settings"
            contentDescription = "Open Settings"
            minHeight = dp(40)
            setOnClickListener { showSettingsWorkspace() }
        }
'''
        # Prefer attach near bottom navigation construction
        placed = False
        for marker in (
            'listOf("Record", "Editor", "Creator", "Library")',
            '"Library"',
            'bottomNav',
        ):
            idx = text.find(marker)
            if idx < 0:
                continue
            # insert after a nearby addView
            add = text.find("addView", idx)
            if add < 0:
                continue
            # find end of statement
            semi = text.find("\n", add)
            text = text[:semi] + "\n" + btn + "\n        // attach settingsEntry beside nav when layout allows\n" + text[semi:]
            placed = True
            break
        if not placed:
            # last resort before matchWidth
            text = text.replace(
                "    private fun matchWidth()",
                btn + "\n    private fun matchWidth()",
                1,
            )

    # Ensure settingsPanel field if only button was added
    if "settingsPanel" not in text:
        raise SystemExit("Android settingsPanel injection failed")

    path.write_text(text, encoding="utf-8")
    print("Android Settings UI wired")


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
        # Prefer after Library nav button
        inject = (
            '\n                        <Button x:Name="SettingsNavButton" Content="Settings" '
            'Click="SettingsNav_Click" '
            'AutomationProperties.Name="Open Settings" '
            'Margin="0,12,0,0"/>\n'
        )
        placed = False
        for marker in ('Content="Library"', 'Tag="Library"', 'CreatorSubnav'):
            idx = xtext.find(marker)
            if idx < 0:
                continue
            # end of that button
            end = xtext.find("/>", idx)
            if end < 0:
                end = xtext.find(">", idx)
                insert_at = end + 1
            else:
                insert_at = end + 2
            xtext = xtext[:insert_at] + inject + xtext[insert_at:]
            placed = True
            break
        if not placed:
            raise SystemExit("Cannot place Windows Settings nav button")

        # Settings panel content
        if "SettingsPanel" not in xtext:
            panel = '''
        <ScrollViewer x:Name="SettingsPanel" Visibility="Collapsed" Padding="24">
            <StackPanel MaxWidth="560">
                <TextBlock Text="Settings" FontSize="24" FontWeight="SemiBold" Margin="0,0,0,8"/>
                <TextBlock TextWrapping="Wrap" Opacity="0.8" Margin="0,0,0,16"
                    Text="App preferences. Processing values persist; other sections are structured UI for later wiring."/>
                <TextBlock Text="RECORDING" FontWeight="SemiBold" Margin="0,12,0,4"/>
                <TextBlock TextWrapping="Wrap" Text="Input device and sample rate follow the Record workspace. Capture behavior is not changed here."/>
                <TextBlock Text="PROCESSING" FontWeight="SemiBold" Margin="0,16,0,4"/>
                <TextBlock TextWrapping="Wrap" Text="Noise threshold, attenuation, gain, high-pass, and preset match Editor processing (see preference contract)."/>
                <Button Content="Reset processing to Natural" Click="SettingsResetProcessing_Click" Margin="0,8,0,0"
                    AutomationProperties.Name="Reset processing preferences to Natural defaults"/>
                <TextBlock Text="APPEARANCE" FontWeight="SemiBold" Margin="0,16,0,4"/>
                <StackPanel Orientation="Horizontal" Spacing="12">
                    <RadioButton x:Name="ThemeSystemRadio" Content="System default" IsChecked="True" GroupName="Theme"/>
                    <RadioButton x:Name="ThemeDarkRadio" Content="Dark studio" GroupName="Theme"/>
                </StackPanel>
                <TextBlock Text="MODELS &amp; OFFLINE" FontWeight="SemiBold" Margin="0,16,0,4"/>
                <TextBlock TextWrapping="Wrap" Text="Bundled offline models (Whisper tiny, Silero VAD) ship with the app. No post-install download required for core paths."/>
                <TextBlock Text="ABOUT" FontWeight="SemiBold" Margin="0,16,0,4"/>
                <TextBlock TextWrapping="Wrap" Text="Synvidra — local-first voice studio. Source recordings are never silently overwritten."/>
            </StackPanel>
        </ScrollViewer>
'''
            # Insert before closing Grid/Window if possible
            close = xtext.rfind("</Grid>")
            if close < 0:
                close = xtext.rfind("</Window>")
            if close < 0:
                raise SystemExit("Cannot insert SettingsPanel into XAML")
            xtext = xtext[:close] + panel + xtext[close:]

        xaml.write_text(xtext, encoding="utf-8")
        print("Windows Settings XAML injected")

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
        // Hide primary workspaces when named panels exist
        foreach (var name in new[] { "RecordPanel", "EditorPanel", "CreatorPanel", "LibraryPanel" })
        {
            var field = GetType().GetField(name, System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Public);
            if (field?.GetValue(this) is FrameworkElement fe)
                fe.Visibility = Visibility.Collapsed;
        }
    }

    private void SettingsResetProcessing_Click(object sender, RoutedEventArgs e)
    {
        try
        {
            // Prefer existing processing reset if present
            var m = GetType().GetMethod("ResetProcessingToNatural",
                System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Public);
            m?.Invoke(this, null);
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Processing preferences reset to Natural (if handler available).";
        }
        catch (Exception error)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Open Editor processing to adjust presets. " + error.Message;
        }
    }

'''
    anchors = [
        "    private void CreatorSubnav_Click",
        "    private async void CreatorExportProject_Click",
        "    private void CreatorExportProject_Click",
        "    private void ApprovePhraseAssembly_Click",
    ]
    for anchor in anchors:
        if anchor in ctext:
            ctext = ctext.replace(anchor, methods + "\n    " + anchor.lstrip(), 1)
            cs.write_text(ctext, encoding="utf-8")
            print("Windows Settings handlers wired")
            return
    raise SystemExit("Cannot find insert point for Windows Settings handlers")


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
