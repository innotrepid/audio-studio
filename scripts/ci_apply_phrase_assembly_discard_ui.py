#!/usr/bin/env python3
"""Wire discard phrase assembly + persist derivedPath on approve."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
    text = path.read_text(encoding="utf-8")

    # Patch approve to set derivedPath when saving
    if 'assemblyStatus = "approved", updatedUtc = now' in text and "derivedPath" not in text[text.find("fun approvePhraseAssembly"):text.find("fun approvePhraseAssembly")+1200]:
        text = text.replace(
            'if (it.requestId == req.requestId) it.copy(assemblyStatus = "approved", updatedUtc = now) else it',
            'if (it.requestId == req.requestId) it.copy(assemblyStatus = "approved", derivedPath = derived.absolutePath, updatedUtc = now) else it',
            1,
        )
        print("Android approve now sets derivedPath")

    helper = r'''
        fun discardPhraseAssembly() {
            val store = creatorProjectStore ?: return
            val project = activeCreatorProject ?: return
            val req = project.phraseReplacementRequests.lastOrNull {
                it.assemblyStatus == "preview" || it.assemblyStatus == "approved"
            }
            if (req == null) {
                captionStatus.text = "No preview/approved phrase assembly to discard."
                return
            }
            try {
                req.derivedPath?.let { p ->
                    runCatching { java.io.File(p).takeIf { it.isFile }?.delete() }
                }
                // Drop temp preview pieces when present
                val dir = java.io.File(cacheDir, "phrase-assembly")
                listOf("preview", "head", "tail").forEach { prefix ->
                    java.io.File(dir, "$prefix-${req.requestId.take(8)}.wav").takeIf { it.isFile }?.delete()
                }
                val now = java.time.Instant.now().toString()
                val updated = project.phraseReplacementRequests.map {
                    if (it.requestId == req.requestId)
                        it.copy(assemblyStatus = "discarded", derivedPath = null, updatedUtc = now)
                    else it
                }
                activeCreatorProject = store.save(project.copy(phraseReplacementRequests = updated))
                captionStatus.text = "Phrase assembly discarded. Source takes unchanged. assemblyStatus=discarded."
            } catch (error: Exception) {
                captionStatus.text = "Discard failed: ${error.message}"
            }
        }

'''
    if "fun discardPhraseAssembly" not in text:
        anchor = "        fun approvePhraseAssembly()"
        if anchor not in text:
            anchor = "        fun buildPhraseAssemblyPreview()"
        if anchor not in text:
            raise SystemExit("Cannot find approve/build helper for discard insert")
        text = text.replace(anchor, helper + anchor, 1)

    if 'exchangeButton("Discard phrase assembly"' not in text:
        key = 'exchangeButton("Approve phrase assembly"'
        idx = text.find(key)
        if idx < 0:
            key = 'exchangeButton("Build phrase assembly preview"'
            idx = text.find(key)
        if idx < 0:
            raise SystemExit("Cannot find assembly button for discard insert")
        close = text.find("matchWidth())", idx)
        if close < 0:
            raise SystemExit("Cannot find button end for discard")
        insert_at = close + len("matchWidth())")
        block = '''
        creatorCaptionsPanel.addView(exchangeButton("Discard phrase assembly", "assemblyStatus=discarded; clear derivedPath; source takes stay") {
            discardPhraseAssembly()
        }, matchWidth())
'''
        text = text[:insert_at] + "\n" + block + text[insert_at:]

    path.write_text(text, encoding="utf-8")
    print("Android phrase assembly discard wired")


def ensure_windows() -> None:
    path = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml.cs"
    text = path.read_text(encoding="utf-8")

    # Patch approve to set DerivedPath
    if "req.AssemblyStatus = \"approved\";" in text and "req.DerivedPath" not in text:
        text = text.replace(
            'req.AssemblyStatus = "approved";',
            'req.AssemblyStatus = "approved";\n            req.DerivedPath = derived;',
            1,
        )
        print("Windows approve now sets DerivedPath")

    if "DiscardPhraseAssembly_Click" in text:
        path.write_text(text, encoding="utf-8")
        print("Windows phrase assembly discard already present")
        return

    methods = r'''
    private void DiscardPhraseAssembly_Click(object sender, RoutedEventArgs e)
    {
        if (_creatorProject is null || _creatorStore is null)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "No Creator project is loaded.";
            return;
        }
        var req = _creatorProject.PhraseReplacementRequests?.LastOrDefault(r =>
            r.AssemblyStatus == "preview" || r.AssemblyStatus == "approved");
        if (req is null)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "No preview/approved phrase assembly to discard.";
            return;
        }
        try
        {
            if (!string.IsNullOrWhiteSpace(req.DerivedPath) && File.Exists(req.DerivedPath))
            {
                try { File.Delete(req.DerivedPath); } catch { /* best-effort */ }
            }
            var dir = Path.Combine(Path.GetTempPath(), "synvidra-phrase-assembly");
            var id8 = req.RequestId.Length >= 8 ? req.RequestId.Substring(0, 8) : req.RequestId;
            foreach (var prefix in new[] { "preview", "head", "tail" })
            {
                var temp = Path.Combine(dir, $"{prefix}-{id8}.wav");
                if (File.Exists(temp))
                {
                    try { File.Delete(temp); } catch { /* best-effort */ }
                }
            }
            req.AssemblyStatus = "discarded";
            req.DerivedPath = null;
            req.UpdatedUtc = DateTime.UtcNow.ToString("O");
            _creatorStore.Save(_creatorProject);
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text =
                    "Phrase assembly discarded. Source takes unchanged. assemblyStatus=discarded.";
        }
        catch (Exception error)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Discard failed: " + error.Message;
        }
    }

'''
    anchor = "    private void ApprovePhraseAssembly_Click"
    if anchor not in text:
        anchor = "    private async void BuildPhraseAssemblyPreview_Click"
    if anchor not in text:
        anchor = "    private void BuildPhraseAssemblyPreview_Click"
    if anchor not in text:
        raise SystemExit("Cannot find approve/build handler for discard")
    text = text.replace(anchor, methods + "\n    " + anchor.lstrip(), 1)
    path.write_text(text, encoding="utf-8")
    print("Windows phrase assembly discard wired")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_phrase_assembly_discard_ui:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
