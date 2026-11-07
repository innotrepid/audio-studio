#!/usr/bin/env python3
"""Wire Creator project JSON export/import UI (Android + Windows handlers)."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
    text = path.read_text(encoding="utf-8")

    helper = r'''
        fun exportCreatorProjectJson() {
            val store = creatorProjectStore ?: return
            val project = activeCreatorProject ?: return
            try {
                val json = store.exportJson(project)
                val outDir = java.io.File(filesDir, "creator/exports").apply { mkdirs() }
                val out = java.io.File(outDir, "project-${project.projectId.take(8)}.json")
                out.writeText(json)
                captionStatus.text = "Exported project JSON (${out.length()} bytes). Takes remain path references; audio files not copied."
            } catch (error: Exception) {
                captionStatus.text = "Export failed: ${error.message}"
            }
        }

        fun importCreatorProjectJson() {
            val store = creatorProjectStore ?: return
            try {
                val dir = java.io.File(filesDir, "creator/exports")
                val candidate = dir.listFiles()?.filter { it.isFile && it.name.endsWith(".json") }?.maxByOrNull { it.lastModified() }
                if (candidate == null) {
                    captionStatus.text = "No exported project JSON found under creator/exports."
                    return
                }
                val imported = store.importJson(candidate.readText())
                activeCreatorProject = store.save(imported)
                captionStatus.text = "Imported ${candidate.name}. Validation passed; previous project replaced via atomic save."
            } catch (error: Exception) {
                captionStatus.text = "Import rejected (current project unchanged): ${error.message}"
            }
        }

'''
    if "fun exportCreatorProjectJson" not in text:
        # Prefer insert near caption helpers; fall back to exchangeButton definition area
        anchors = [
            "        fun exchangeButton",
            "        fun approvePhraseAssembly",
            "        fun buildPhraseAssemblyPreview",
        ]
        for anchor in anchors:
            if anchor in text:
                text = text.replace(anchor, helper + anchor, 1)
                break
        else:
            raise SystemExit("Cannot find insert point for export/import helpers")

    if 'exchangeButton("Export project JSON"' not in text:
        block = '''
        creatorScriptsPanel.addView(exchangeButton("Export project JSON", "Write validated project JSON; take paths only, no audio copy") {
            exportCreatorProjectJson()
        }, matchWidth())
        creatorScriptsPanel.addView(exchangeButton("Import project JSON", "Load last export under creator/exports after validation") {
            importCreatorProjectJson()
        }, matchWidth())
'''
        # Prefer Scripts panel; fall back to Captions
        if "creatorScriptsPanel.addView" in text:
            # Insert after first creatorScriptsPanel.addView block end is hard; place before Captions subnav if needed
            key = "creatorScriptsPanel"
            idx = text.find(key)
            # Find a safe later Scripts button area: "Create from template" or similar
            for marker in (
                'exchangeButton("Create from template"',
                'creatorScriptsPanel.addView(exchangeButton',
            ):
                m = text.find(marker, idx)
                if m >= 0:
                    close = text.find("matchWidth())", m)
                    if close >= 0:
                        insert_at = close + len("matchWidth())")
                        text = text[:insert_at] + "\n" + block + text[insert_at:]
                        break
            else:
                # Append near creatorScriptsPanel first use
                close = text.find("matchWidth())", idx)
                if close < 0:
                    raise SystemExit("Cannot place Android export/import buttons")
                insert_at = close + len("matchWidth())")
                text = text[:insert_at] + "\n" + block + text[insert_at:]
        else:
            raise SystemExit("creatorScriptsPanel missing")

    path.write_text(text, encoding="utf-8")
    print("Android project backup UI wired")


def ensure_windows() -> None:
    path = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml.cs"
    text = path.read_text(encoding="utf-8")
    if "CreatorExportProject_Click" in text and "ExportJson" in text:
        print("Windows project export/import handlers already present")
        return

    methods = r'''
    private async void CreatorExportProject_Click(object sender, RoutedEventArgs e)
    {
        if (_creatorProject is null || _creatorStore is null)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "No Creator project is loaded.";
            return;
        }
        try
        {
            var json = _creatorStore.ExportJson(_creatorProject);
            var picker = new Windows.Storage.Pickers.FileSavePicker();
            var hwnd = WinRT.Interop.WindowNative.GetWindowHandle(this);
            WinRT.Interop.InitializeWithWindow.Initialize(picker, hwnd);
            picker.SuggestedStartLocation = Windows.Storage.Pickers.PickerLocationId.DocumentsLibrary;
            picker.FileTypeChoices.Add("JSON", new System.Collections.Generic.List<string> { ".json" });
            picker.SuggestedFileName = "creator-project";
            var file = await picker.PickSaveFileAsync();
            if (file is null) return;
            await Windows.Storage.FileIO.WriteTextAsync(file, json);
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text =
                    $"Exported project JSON to {file.Name}. Take paths only; audio files not copied.";
        }
        catch (Exception error)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Export failed: " + error.Message;
        }
    }

    private async void CreatorImportProject_Click(object sender, RoutedEventArgs e)
    {
        if (_creatorStore is null)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Creator store is not ready.";
            return;
        }
        try
        {
            var picker = new Windows.Storage.Pickers.FileOpenPicker();
            var hwnd = WinRT.Interop.WindowNative.GetWindowHandle(this);
            WinRT.Interop.InitializeWithWindow.Initialize(picker, hwnd);
            picker.SuggestedStartLocation = Windows.Storage.Pickers.PickerLocationId.DocumentsLibrary;
            picker.FileTypeFilter.Add(".json");
            var file = await picker.PickSingleFileAsync();
            if (file is null) return;
            var json = await Windows.Storage.FileIO.ReadTextAsync(file);
            var imported = _creatorStore.ImportJson(json);
            _creatorStore.Save(imported);
            _creatorProject = imported;
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text =
                    $"Imported {file.Name}. Validation passed; project replaced via atomic save.";
        }
        catch (Exception error)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text =
                    "Import rejected (current project unchanged): " + error.Message;
        }
    }

'''
    if "CreatorExportProject_Click" not in text:
        anchors = [
            "    private void ApprovePhraseAssembly_Click",
            "    private async void BuildPhraseAssemblyPreview_Click",
            "    private void BuildPhraseAssemblyPreview_Click",
            "    private void CreatorSubnav_Click",
        ]
        for anchor in anchors:
            if anchor in text:
                text = text.replace(anchor, methods + "\n    " + anchor.lstrip(), 1)
                break
        else:
            raise SystemExit("Cannot find insert point for Windows import/export handlers")

    path.write_text(text, encoding="utf-8")
    print("Windows project backup handlers wired")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_project_backup_ui:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
