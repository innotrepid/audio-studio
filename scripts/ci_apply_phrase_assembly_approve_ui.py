#!/usr/bin/env python3
"""Wire Captions phrase assembly approve (derived WAV, assemblyStatus=approved)."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

APPROVE_BUTTON = '''
        creatorCaptionsPanel.addView(exchangeButton("Approve phrase assembly", "Copy preview to durable WAV; assemblyStatus=approved") {
            approvePhraseAssembly()
        }, matchWidth())
'''


def ensure_android() -> None:
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
    text = path.read_text(encoding="utf-8")

    # Remove a previously mis-placed Approve button (matched status string, not UI button).
    misplaced = re.search(
        r'\n\s*creatorCaptionsPanel\.addView\(exchangeButton\("Approve phrase assembly".*?matchWidth\(\)\)\n',
        text,
        flags=re.DOTALL,
    )
    if misplaced:
        # Keep only if it sits after the Build preview exchangeButton.
        build_btn = text.find('exchangeButton("Build phrase assembly preview"')
        if build_btn < 0 or misplaced.start() < build_btn:
            text = text[: misplaced.start()] + text[misplaced.end() :]
            print("Removed misplaced Approve phrase assembly button")

    helper = r'''
        fun approvePhraseAssembly() {
            val store = creatorProjectStore ?: return
            val project = activeCreatorProject ?: return
            val req = project.phraseReplacementRequests.lastOrNull {
                it.assemblyStatus == "preview" &&
                    it.alignmentStatus == "manual" &&
                    !it.replacementTakeId.isNullOrBlank()
            }
            if (req == null) {
                captionStatus.text = "Run Build phrase assembly first (need assemblyStatus=preview)."
                return
            }
            val preview = java.io.File(cacheDir, "phrase-assembly/preview-${req.requestId.take(8)}.wav")
            if (!preview.isFile || preview.length() <= 44L) {
                captionStatus.text = "Preview WAV missing. Rebuild the phrase assembly temp WAV."
                return
            }
            try {
                val outDir = java.io.File(filesDir, "phrase-assembly-approved").apply { mkdirs() }
                val derived = java.io.File(outDir, "approved-${req.requestId}.wav")
                preview.copyTo(derived, overwrite = true)
                if (!derived.isFile || derived.length() <= 44L) {
                    captionStatus.text = "Could not write derived approved WAV."
                    return
                }
                val now = java.time.Instant.now().toString()
                val updated = project.phraseReplacementRequests.map {
                    if (it.requestId == req.requestId) it.copy(assemblyStatus = "approved", updatedUtc = now) else it
                }
                activeCreatorProject = store.save(project.copy(phraseReplacementRequests = updated))
                captionStatus.text =
                    "Assembly approved. Derived WAV: ${derived.name} (source takes unchanged). assemblyStatus=approved."
            } catch (error: Exception) {
                captionStatus.text = "Approve failed: ${error.message}"
            }
        }

'''
    if "fun approvePhraseAssembly" not in text:
        anchor = "        fun buildPhraseAssemblyPreview()"
        if anchor not in text:
            anchor = "        fun playPhraseOriginalRange()"
        if anchor not in text:
            raise SystemExit("Cannot find assembly helper for approve insert")
        text = text.replace(anchor, helper + anchor, 1)
    else:
        # Refresh helper messages to avoid ambiguous "Build phrase assembly preview" substrings
        text = text.replace(
            'captionStatus.text = "Build an assembly preview first (assemblyStatus=preview)."',
            'captionStatus.text = "Run Build phrase assembly first (need assemblyStatus=preview)."',
        )
        text = text.replace(
            'captionStatus.text = "Preview WAV missing. Build phrase assembly preview again."',
            'captionStatus.text = "Preview WAV missing. Rebuild the phrase assembly temp WAV."',
        )

    if 'exchangeButton("Approve phrase assembly"' not in text:
        key = 'exchangeButton("Build phrase assembly preview"'
        idx = text.find(key)
        if idx < 0:
            raise SystemExit("Cannot find assembly preview exchangeButton for approve insert")
        close = text.find("matchWidth())", idx)
        if close < 0:
            raise SystemExit("Cannot find button end for approve")
        insert_at = close + len("matchWidth())")
        text = text[:insert_at] + "\n" + APPROVE_BUTTON + text[insert_at:]

    path.write_text(text, encoding="utf-8")
    print("Android phrase assembly approve wired")


def ensure_windows() -> None:
    path = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml.cs"
    text = path.read_text(encoding="utf-8")
    if "ApprovePhraseAssembly_Click" in text:
        print("Windows phrase assembly approve already present")
        return

    methods = r'''
    private void ApprovePhraseAssembly_Click(object sender, RoutedEventArgs e)
    {
        if (_creatorProject is null || _creatorStore is null)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "No Creator project is loaded.";
            return;
        }
        var req = _creatorProject.PhraseReplacementRequests?.LastOrDefault(r =>
            r.AssemblyStatus == "preview" &&
            r.AlignmentStatus == "manual" &&
            !string.IsNullOrWhiteSpace(r.ReplacementTakeId));
        if (req is null)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Run Build phrase assembly first (need assemblyStatus=preview).";
            return;
        }
        try
        {
            var dir = Path.Combine(Path.GetTempPath(), "synvidra-phrase-assembly");
            var id8 = req.RequestId.Length >= 8 ? req.RequestId.Substring(0, 8) : req.RequestId;
            var preview = Path.Combine(dir, $"preview-{id8}.wav");
            if (!File.Exists(preview) || new FileInfo(preview).Length <= 44)
            {
                if (CreatorCaptionStatusText is not null)
                    CreatorCaptionStatusText.Text = "Preview WAV missing. Rebuild the phrase assembly temp WAV.";
                return;
            }
            var outDir = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "Synvidra", "phrase-assembly-approved");
            Directory.CreateDirectory(outDir);
            var derived = Path.Combine(outDir, $"approved-{req.RequestId}.wav");
            File.Copy(preview, derived, overwrite: true);
            if (!File.Exists(derived) || new FileInfo(derived).Length <= 44)
            {
                if (CreatorCaptionStatusText is not null)
                    CreatorCaptionStatusText.Text = "Could not write derived approved WAV.";
                return;
            }
            req.AssemblyStatus = "approved";
            req.UpdatedUtc = DateTime.UtcNow.ToString("O");
            _creatorStore.Save(_creatorProject);
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text =
                    $"Assembly approved. Derived WAV: {Path.GetFileName(derived)} (source takes unchanged). assemblyStatus=approved.";
        }
        catch (Exception error)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Approve failed: " + error.Message;
        }
    }

'''
    anchor = "    private async void BuildPhraseAssemblyPreview_Click"
    if anchor not in text:
        anchor = "    private void BuildPhraseAssemblyPreview_Click"
    if anchor not in text:
        anchor = "    private async void PlayPhraseOriginalRange_Click"
    if anchor not in text:
        raise SystemExit("Cannot find assembly handler for approve insert")
    text = text.replace(anchor, methods + "\n    " + anchor.lstrip(), 1)
    path.write_text(text, encoding="utf-8")
    print("Windows phrase assembly approve handler wired")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_phrase_assembly_approve_ui:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
