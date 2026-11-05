#!/usr/bin/env python3
"""Wire teleprompter reading-position persistence into Android and Windows UI."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
    text = path.read_text(encoding="utf-8")
    original = text

    if "saveTeleprompterReadingPosition" in text and "restoreTeleprompterReadingPosition" in text:
        print("Android reading-position hooks already present")
        return

    if "private var selectedCreatorSectionId" not in text:
        anchor = "    private var teleprompterScrollRemainder = 0f\n"
        if anchor not in text:
            raise SystemExit("Cannot find teleprompterScrollRemainder field")
        text = text.replace(
            anchor,
            anchor
            + "    private var selectedCreatorSectionId: String? = null\n"
            + "    private var selectedCreatorScriptId: String? = null\n",
            1,
        )
        text = text.replace(
            "        var selectedCreatorSectionId: String? = null\n",
            "        // selectedCreatorSectionId is a class field (reading-position + section nav)\n",
            1,
        )
        print("Promoted selectedCreatorSectionId to class field")

    helpers = '''
    private fun saveTeleprompterReadingPosition() {
        val scrollY = teleprompterScrollView?.scrollY ?: 0
        val sectionId = selectedCreatorSectionId
        if (sectionId.isNullOrBlank()) return
        val scriptId = selectedCreatorScriptId
            ?: activeCreatorProject?.scripts?.firstOrNull { script ->
                script.sections.any { it.sectionId == sectionId }
            }?.scriptId
        TeleprompterPreferences.saveReadingPosition(
            this,
            sectionId,
            scriptId,
            scrollY,
        )
    }

    private fun restoreTeleprompterReadingPosition() {
        val saved = TeleprompterPreferences.loadReadingPosition(this)
        val sectionId = selectedCreatorSectionId
        if (saved == null || sectionId.isNullOrBlank() || saved.sectionId != sectionId) {
            teleprompterScrollView?.scrollTo(0, 0)
            return
        }
        val scriptId = selectedCreatorScriptId
            ?: activeCreatorProject?.scripts?.firstOrNull { script ->
                script.sections.any { it.sectionId == sectionId }
            }?.scriptId
        if (!scriptId.isNullOrBlank()
            && saved.scriptId.isNotBlank()
            && saved.scriptId != scriptId
        ) {
            teleprompterScrollView?.scrollTo(0, 0)
            return
        }
        teleprompterScrollView?.post {
            teleprompterScrollView?.scrollTo(0, saved.scrollY.coerceAtLeast(0))
        }
    }

'''

    if "private fun saveTeleprompterReadingPosition" not in text:
        insert_at = text.find("    private fun startTeleprompterTake(")
        if insert_at < 0:
            raise SystemExit("Cannot find startTeleprompterTake anchor")
        text = text[:insert_at] + helpers + text[insert_at:]
        print("Injected Android reading-position helpers")

    text = text.replace(
        "        teleprompterScrollView?.scrollTo(0, 0)\n"
        "        teleprompterStartButton?.isEnabled = true\n",
        "        restoreTeleprompterReadingPosition()\n"
        "        teleprompterStartButton?.isEnabled = true\n",
        1,
    )

    pause_part = text.split("private fun toggleTeleprompterPause")[1][:900] if "private fun toggleTeleprompterPause" in text else ""
    if "saveTeleprompterReadingPosition()" not in pause_part:
        text = text.replace(
            "            teleprompterPauseButton?.text = \"Resume take\"\n"
            "            teleprompterCountdownText?.text = \"Take and script paused.\"\n",
            "            saveTeleprompterReadingPosition()\n"
            "            teleprompterPauseButton?.text = \"Resume take\"\n"
            "            teleprompterCountdownText?.text = \"Take and script paused.\"\n",
            1,
        )

    text = text.replace(
        "    private fun stopTeleprompterScroll() {\n"
        "        mainHandler.removeCallbacks(teleprompterScrollClock)\n",
        "    private fun stopTeleprompterScroll() {\n"
        "        saveTeleprompterReadingPosition()\n"
        "        mainHandler.removeCallbacks(teleprompterScrollClock)\n",
        1,
    )

    on_stop = text[text.find("override fun onStop") : text.find("override fun onStop") + 700] if "override fun onStop" in text else ""
    if "saveTeleprompterReadingPosition()" not in on_stop:
        text = text.replace(
            "        if (recording) stopRecording(interrupted = true)\n"
            "        // Processing belongs to this Activity. Cancel it when leaving the foreground;\n",
            "        saveTeleprompterReadingPosition()\n"
            "        if (recording) stopRecording(interrupted = true)\n"
            "        // Processing belongs to this Activity. Cancel it when leaving the foreground;\n",
            1,
        )

    # Stamp script id from the active script when a section is chosen (Section has no scriptId field)
    if "selectedCreatorScriptId =" not in text:
        text = text.replace(
            "            selectedCreatorSectionId = ordered.first().sectionId\n",
            "            selectedCreatorSectionId = ordered.first().sectionId\n"
            "            selectedCreatorScriptId = currentCreatorScript()?.scriptId\n",
            1,
        )

    if text != original:
        path.write_text(text, encoding="utf-8")
        print("Wrote Android MainActivity reading-position wiring")
    else:
        print("Android MainActivity unchanged")


def ensure_windows() -> None:
    path = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml.cs"
    text = path.read_text(encoding="utf-8")
    original = text

    if "SaveTeleprompterReadingPosition" in text and "RestoreTeleprompterReadingPosition" in text:
        print("Windows reading-position hooks already present")
        return

    methods = '''
    private void SaveTeleprompterReadingPosition()
    {
        try
        {
            var sectionId = SelectedCreatorSectionId;
            if (string.IsNullOrWhiteSpace(sectionId))
                return;
            var scriptId = SelectedCreatorScript?.ScriptId;
            var scrollY = TeleprompterScriptScroll?.VerticalOffset ?? 0;
            TeleprompterPreferences.SaveReadingPosition(sectionId, scriptId, scrollY);
        }
        catch
        {
            // Preference write must not interrupt recording teardown.
        }
    }

    private void RestoreTeleprompterReadingPosition()
    {
        try
        {
            var saved = TeleprompterPreferences.LoadReadingPosition();
            if (saved is null || string.IsNullOrWhiteSpace(saved.SectionId))
            {
                TeleprompterScriptScroll?.ChangeView(null, 0, null, true);
                return;
            }
            var sectionId = SelectedCreatorSectionId;
            if (string.IsNullOrWhiteSpace(sectionId) || !string.Equals(saved.SectionId, sectionId, StringComparison.Ordinal))
            {
                TeleprompterScriptScroll?.ChangeView(null, 0, null, true);
                return;
            }
            var scriptId = SelectedCreatorScript?.ScriptId;
            if (!string.IsNullOrWhiteSpace(scriptId)
                && !string.IsNullOrWhiteSpace(saved.ScriptId)
                && !string.Equals(saved.ScriptId, scriptId, StringComparison.Ordinal))
            {
                TeleprompterScriptScroll?.ChangeView(null, 0, null, true);
                return;
            }
            TeleprompterScriptScroll?.ChangeView(null, saved.ScrollY, null, true);
        }
        catch
        {
            TeleprompterScriptScroll?.ChangeView(null, 0, null, true);
        }
    }

'''

    if "private void SaveTeleprompterReadingPosition()" not in text:
        for anchor in (
            "    private void StartTeleprompterScroll()",
            "    private void StopTeleprompterScroll()",
            "    private void ResetTeleprompterControls()",
        ):
            if anchor in text:
                text = text.replace(anchor, methods + anchor, 1)
                print("Injected Windows reading-position helpers")
                break
        else:
            raise SystemExit("Cannot find Windows teleprompter method anchor")

    if "SelectedCreatorSectionId" not in text:
        prop = '''
    private string? SelectedCreatorSectionId
    {
        get
        {
            var script = SelectedCreatorScript;
            if (script?.Sections is null || script.Sections.Count == 0)
                return null;
            var ordered = script.Sections.OrderBy(section => section.Order).ToList();
            var index = CreatorSectionComboBox?.SelectedIndex ?? 0;
            if (index < 0 || index >= ordered.Count)
                index = 0;
            return ordered[index].SectionId;
        }
    }

'''
        text = text.replace(
            "    private void SaveTeleprompterReadingPosition()",
            prop + "    private void SaveTeleprompterReadingPosition()",
            1,
        )
        print("Injected SelectedCreatorSectionId helper property")

    text = text.replace(
        "        TeleprompterScriptScroll.ChangeView(null, 0, null, true);\n"
        "        StartStudioTakeButton.Content = \"Cancel countdown\";\n",
        "        RestoreTeleprompterReadingPosition();\n"
        "        StartStudioTakeButton.Content = \"Cancel countdown\";\n",
        1,
    )

    text = text.replace(
        "    private void StartTeleprompterScroll()\n"
        "    {\n"
        "        _teleprompterPaused = false;\n"
        "        TeleprompterScriptScroll.ChangeView(null, 0, null, true);\n",
        "    private void StartTeleprompterScroll()\n"
        "    {\n"
        "        _teleprompterPaused = false;\n",
        1,
    )

    text = text.replace(
        "    private void StopTeleprompterScroll()\n"
        "    {\n"
        "        _teleprompterScrollTimer.Stop();\n",
        "    private void StopTeleprompterScroll()\n"
        "    {\n"
        "        SaveTeleprompterReadingPosition();\n"
        "        _teleprompterScrollTimer.Stop();\n",
        1,
    )

    pause_part = text.split("PauseTeleprompter_Click")[1][:900] if "PauseTeleprompter_Click" in text else ""
    if "SaveTeleprompterReadingPosition()" not in pause_part:
        text = text.replace(
            "            PauseTeleprompterButton.Content = \"Resume take\";\n",
            "            SaveTeleprompterReadingPosition();\n"
            "            PauseTeleprompterButton.Content = \"Resume take\";\n",
            1,
        )

    if text != original:
        path.write_text(text, encoding="utf-8")
        print("Wrote Windows MainWindow reading-position wiring")
    else:
        print("Windows MainWindow unchanged")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_reading_position:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
