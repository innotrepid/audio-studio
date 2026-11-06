#!/usr/bin/env python3
"""Wire Prev/Next section navigation on Record studio teleprompter."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
    text = path.read_text(encoding="utf-8")
    original = text

    if "navigateTeleprompterSection" in text and "teleprompterPrevSectionButton" in text:
        print("Android section-nav hooks already present")
        return

    # Ensure section id is a class field (reading-position apply may already promote it)
    if "private var selectedCreatorSectionId" not in text:
        anchor = "    private var selectedCreatorScriptId: String? = null\n"
        if anchor in text:
            text = text.replace(
                anchor,
                anchor + "    private var selectedCreatorSectionId: String? = null\n",
                1,
            )
        else:
            remainder = "    private var teleprompterScrollRemainder = 0f\n"
            if remainder not in text:
                raise SystemExit("Cannot promote selectedCreatorSectionId")
            text = text.replace(
                remainder,
                remainder + "    private var selectedCreatorSectionId: String? = null\n",
                1,
            )
        text = text.replace(
            "        var selectedCreatorSectionId: String? = null\n",
            "        // selectedCreatorSectionId is a class field\n",
            1,
        )

    if "private var teleprompterPrevSectionButton" not in text:
        field_anchor = "    private var teleprompterPauseButton: Button? = null\n"
        if field_anchor not in text:
            raise SystemExit("Cannot find teleprompterPauseButton field")
        text = text.replace(
            field_anchor,
            field_anchor
            + "    private var teleprompterPrevSectionButton: Button? = null\n"
            + "    private var teleprompterNextSectionButton: Button? = null\n"
            + "    private var teleprompterSectionLabel: TextView? = null\n",
            1,
        )

    helpers = '''
    private fun orderedCreatorSections(): List<CreatorProjectStore.Section> {
        val project = activeCreatorProject ?: return emptyList()
        val script = project.scripts.firstOrNull { script ->
            !selectedCreatorScriptId.isNullOrBlank() && script.scriptId == selectedCreatorScriptId
        } ?: project.scripts.firstOrNull() ?: return emptyList()
        return script.sections.sortedBy { it.order }
    }

    private fun navigateTeleprompterSection(delta: Int) {
        val ordered = orderedCreatorSections()
        if (ordered.isEmpty()) {
            statusText.text = "Add a script section under Creator before navigating."
            return
        }
        if ("saveTeleprompterReadingPosition" in "" /* marker */) Unit
        try {
            // Prefer reading-position helper when CI applied it
            val save = this::class.java.getDeclaredMethod("saveTeleprompterReadingPosition")
            save.isAccessible = true
            save.invoke(this)
        } catch (_: Exception) {
            // optional
        }
        val currentId = selectedCreatorSectionId
        val currentIndex = ordered.indexOfFirst { it.sectionId == currentId }.let { if (it < 0) 0 else it }
        val nextIndex = (currentIndex + delta).coerceIn(0, ordered.lastIndex)
        if (nextIndex == currentIndex && ordered.size == 1) {
            teleprompterCountdownText?.text = "Only one section in this script."
            return
        }
        if (nextIndex == currentIndex) {
            teleprompterCountdownText?.text = if (delta < 0) "Already at the first section." else "Already at the last section."
            return
        }
        val section = ordered[nextIndex]
        selectedCreatorSectionId = section.sectionId
        val script = activeCreatorProject?.scripts?.firstOrNull { s ->
            s.sections.any { it.sectionId == section.sectionId }
        }
        selectedCreatorScriptId = script?.scriptId ?: selectedCreatorScriptId
        val body = section.text.trim().ifBlank { "This section is empty. Add text under Creator \u2192 Scripts." }
        teleprompterScriptView?.text = body
        teleprompterSectionLabel?.text = "Section ${'$'}{nextIndex + 1} / ${'$'}{ordered.size} \u00b7 ${'$'}{section.title}"
        teleprompterPrevSectionButton?.isEnabled = nextIndex > 0
        teleprompterNextSectionButton?.isEnabled = nextIndex < ordered.lastIndex
        try {
            val restore = this::class.java.getDeclaredMethod("restoreTeleprompterReadingPosition")
            restore.isAccessible = true
            restore.invoke(this)
        } catch (_: Exception) {
            teleprompterScrollView?.scrollTo(0, 0)
        }
        teleprompterCountdownText?.text = "Showing section \u201c${'$'}{section.title}\u201d (${'$'}{nextIndex + 1}/${'$'}{ordered.size})."
        statusText.text = "Teleprompter section: ${'$'}{section.title}"
    }

    private fun refreshTeleprompterSectionChrome() {
        val ordered = orderedCreatorSections()
        if (ordered.isEmpty()) {
            teleprompterSectionLabel?.text = "No sections"
            teleprompterPrevSectionButton?.isEnabled = false
            teleprompterNextSectionButton?.isEnabled = false
            return
        }
        val currentId = selectedCreatorSectionId
        val index = ordered.indexOfFirst { it.sectionId == currentId }.let { if (it < 0) 0 else it }
        val section = ordered[index]
        teleprompterSectionLabel?.text = "Section ${'$'}{index + 1} / ${'$'}{ordered.size} \u00b7 ${'$'}{section.title}"
        teleprompterPrevSectionButton?.isEnabled = index > 0
        teleprompterNextSectionButton?.isEnabled = index < ordered.lastIndex
    }

'''

    # Fix helpers: reflection is ugly; call save/restore by name if present after string inject
    helpers = helpers.replace(
        '''        if ("saveTeleprompterReadingPosition" in "" /* marker */) Unit
        try {
            // Prefer reading-position helper when CI applied it
            val save = this::class.java.getDeclaredMethod("saveTeleprompterReadingPosition")
            save.isAccessible = true
            save.invoke(this)
        } catch (_: Exception) {
            // optional
        }
''',
        """        // Reading-position helper is injected by ci_apply_reading_position when present.
        runCatching { saveTeleprompterReadingPosition() }
""",
    ).replace(
        '''        try {
            val restore = this::class.java.getDeclaredMethod("restoreTeleprompterReadingPosition")
            restore.isAccessible = true
            restore.invoke(this)
        } catch (_: Exception) {
            teleprompterScrollView?.scrollTo(0, 0)
        }
''',
        """        runCatching { restoreTeleprompterReadingPosition() }
            .onFailure { teleprompterScrollView?.scrollTo(0, 0) }
""",
    )

    if "private fun navigateTeleprompterSection" not in text:
        insert_at = text.find("    private fun startTeleprompterTake(")
        if insert_at < 0:
            insert_at = text.find("    private fun startTeleprompterScroll(")
        if insert_at < 0:
            raise SystemExit("Cannot find insert point for section nav helpers")
        text = text[:insert_at] + helpers + text[insert_at:]
        print("Injected Android section-nav helpers")

    # UI: section row after teleprompterControls
    if "teleprompterPrevSectionButton" not in text or "Section nav" not in text:
        ui = '''
        val teleprompterSectionNav = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(0, dp(4), 0, dp(4))
        }
        val teleprompterPrevSection = Button(this).apply {
            text = "\u25C0 Section"
            minHeight = dp(40)
            setOnClickListener { navigateTeleprompterSection(-1) }
        }.also { teleprompterPrevSectionButton = it }
        val teleprompterNextSection = Button(this).apply {
            text = "Section \u25B6"
            minHeight = dp(40)
            setOnClickListener { navigateTeleprompterSection(1) }
        }.also { teleprompterNextSectionButton = it }
        val teleprompterSectionStatus = TextView(this).apply {
            text = "Section"
            textSize = 12f
            setTextColor(Color.rgb(190, 198, 214))
            setPadding(dp(8), 0, dp(8), 0)
        }.also { teleprompterSectionLabel = it }
        teleprompterSectionNav.addView(teleprompterPrevSection, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f).apply {
            marginEnd = dp(4)
        })
        teleprompterSectionNav.addView(teleprompterSectionStatus, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.4f))
        teleprompterSectionNav.addView(teleprompterNextSection, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f).apply {
            marginStart = dp(4)
        })
'''
        # insert before recordSection.addView(teleprompterControls
        needle = "        recordSection.addView(teleprompterControls, matchWidth())\n"
        if needle not in text:
            raise SystemExit("Cannot find teleprompterControls addView")
        text = text.replace(
            needle,
            ui + needle + "        recordSection.addView(teleprompterSectionNav, matchWidth())\n",
            1,
        )
        print("Injected Android section-nav UI")

    # Refresh chrome when creator sections refresh
    if "refreshTeleprompterSectionChrome()" not in text:
        text = text.replace(
            "            teleprompterScriptView?.text = creatorBodyInput.text?.toString().orEmpty().ifBlank { \"This section is empty. Add text under Creator \u2192 Scripts.\" }\n",
            "            teleprompterScriptView?.text = creatorBodyInput.text?.toString().orEmpty().ifBlank { \"This section is empty. Add text under Creator \u2192 Scripts.\" }\n"
            "            refreshTeleprompterSectionChrome()\n",
            1,
        )

    if text != original:
        path.write_text(text, encoding="utf-8")
        print("Wrote Android section-nav wiring")
    else:
        print("Android section-nav unchanged")


def ensure_windows() -> None:
    path = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml.cs"
    text = path.read_text(encoding="utf-8")
    original = text

    if "NavigateTeleprompterSection" in text:
        print("Windows section-nav hooks already present")
        return

    methods = '''
    private void NavigateTeleprompterSection(int delta)
    {
        var script = SelectedCreatorScript;
        if (script?.Sections is null || script.Sections.Count == 0)
        {
            if (TeleprompterStatusText != null)
                TeleprompterStatusText.Text = "Add a script section under Creator before navigating.";
            return;
        }

        try { SaveTeleprompterReadingPosition(); } catch { /* optional */ }

        var ordered = script.Sections.OrderBy(section => section.Order).ToList();
        var index = CreatorSectionComboBox?.SelectedIndex ?? 0;
        if (index < 0 || index >= ordered.Count)
            index = 0;
        var next = Math.Clamp(index + delta, 0, ordered.Count - 1);
        if (next == index)
        {
            if (TeleprompterStatusText != null)
                TeleprompterStatusText.Text = delta < 0
                    ? "Already at the first section."
                    : "Already at the last section.";
            UpdateTeleprompterSectionChrome();
            return;
        }

        _loadingCreatorEditor = true;
        if (CreatorSectionComboBox != null)
            CreatorSectionComboBox.SelectedIndex = next;
        _loadingCreatorEditor = false;

        var section = ordered[next];
        if (CreatorScriptBodyBox != null)
            CreatorScriptBodyBox.Text = section.Text;
        if (TeleprompterScriptText != null)
            TeleprompterScriptText.Text = string.IsNullOrWhiteSpace(section.Text)
                ? "This section is empty. Add text under Creator \u2192 Scripts."
                : section.Text;
        try { RestoreTeleprompterReadingPosition(); } catch
        {
            TeleprompterScriptScroll?.ChangeView(null, 0, null, true);
        }
        UpdateTeleprompterSectionChrome();
        if (TeleprompterStatusText != null)
            TeleprompterStatusText.Text = $"Showing section \u201c{section.Title}\u201d ({next + 1}/{ordered.Count}).";
    }

    private void UpdateTeleprompterSectionChrome()
    {
        var script = SelectedCreatorScript;
        var ordered = script?.Sections?.OrderBy(section => section.Order).ToList() ?? new List<CreatorProjectStore.SectionDocument>();
        var index = CreatorSectionComboBox?.SelectedIndex ?? 0;
        if (ordered.Count == 0)
        {
            if (TeleprompterSectionLabel != null)
                TeleprompterSectionLabel.Text = "No sections";
            if (TeleprompterPrevSectionButton != null)
                TeleprompterPrevSectionButton.IsEnabled = false;
            if (TeleprompterNextSectionButton != null)
                TeleprompterNextSectionButton.IsEnabled = false;
            return;
        }
        if (index < 0 || index >= ordered.Count)
            index = 0;
        var section = ordered[index];
        if (TeleprompterSectionLabel != null)
            TeleprompterSectionLabel.Text = $"Section {index + 1} / {ordered.Count} \u00b7 {section.Title}";
        if (TeleprompterPrevSectionButton != null)
            TeleprompterPrevSectionButton.IsEnabled = index > 0;
        if (TeleprompterNextSectionButton != null)
            TeleprompterNextSectionButton.IsEnabled = index < ordered.Count - 1;
    }

    private void TeleprompterPrevSection_Click(object sender, RoutedEventArgs e) => NavigateTeleprompterSection(-1);

    private void TeleprompterNextSection_Click(object sender, RoutedEventArgs e) => NavigateTeleprompterSection(1);

'''

    for anchor in (
        "    private void StartTeleprompterScroll()",
        "    private void StopTeleprompterScroll()",
        "    private void PauseTeleprompter_Click",
    ):
        if anchor in text:
            text = text.replace(anchor, methods + anchor, 1)
            print("Injected Windows section-nav helpers")
            break
    else:
        raise SystemExit("Cannot find Windows teleprompter anchor")

    # Refresh chrome when sections refresh
    if "UpdateTeleprompterSectionChrome();" not in text:
        text = text.replace(
            "        TeleprompterScriptText.Text = ordered[index].Text;\n",
            "        TeleprompterScriptText.Text = ordered[index].Text;\n"
            "        UpdateTeleprompterSectionChrome();\n",
            1,
        )

    if text != original:
        path.write_text(text, encoding="utf-8")
        print("Wrote Windows section-nav wiring")
    else:
        print("Windows section-nav unchanged")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_section_nav:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
