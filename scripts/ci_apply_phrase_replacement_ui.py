#!/usr/bin/env python3
"""Wire Captions UI to create phrase replacement requests (selection only)."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
    text = path.read_text(encoding="utf-8")

    # Heal wrong binding from earlier apply
    if "creatorTranscriptInput" in text:
        text = text.replace("creatorTranscriptInput", "transcriptInput")
        path.write_text(text, encoding="utf-8")
        print("Healed creatorTranscriptInput -> transcriptInput")
        text = path.read_text(encoding="utf-8")

    if "Save phrase for replacement" in text and "fun savePhraseReplacementRequest" in text:
        # also avoid project.transcriptTakeId if property may be missing on some intermediate patches
        if "project.transcriptTakeId" in text and "transcriptTakeId" not in (
            path.parent / "CreatorProjectStore.kt"
        ).read_text(encoding="utf-8"):
            text = text.replace(
                "val sourceTake = project.transcriptTakeId\n"
                "                    ?: project.takes.firstOrNull()?.optString(\"takeId\")?.takeIf { it.isNotBlank() }",
                "val sourceTake = project.takes.firstOrNull()?.optString(\"takeId\")?.takeIf { it.isNotBlank() }",
            )
            path.write_text(text, encoding="utf-8")
            print("Healed sourceTake without transcriptTakeId")
        else:
            print("Android phrase replacement UI already present")
        return

    helper = '''
        fun savePhraseReplacementRequest() {
            val store = creatorProjectStore ?: return
            val project = activeCreatorProject ?: return
            val full = transcriptInput.text?.toString() ?: project.transcript
            val start = transcriptInput.selectionStart
            val end = transcriptInput.selectionEnd
            val phrase: String
            val charStart: Int?
            val charEnd: Int?
            if (start >= 0 && end > start) {
                phrase = full.substring(start, end).trim()
                charStart = start
                charEnd = end
            } else {
                phrase = full.trim()
                charStart = null
                charEnd = null
            }
            if (phrase.isBlank()) {
                captionStatus.text = "Select phrase text in the transcript, or enter a transcript first."
                return
            }
            try {
                val now = java.time.Instant.now().toString()
                val sourceTake = project.takes.firstOrNull()?.optString("takeId")?.takeIf { it.isNotBlank() }
                val req = CreatorProjectStore.PhraseReplacementRequest(
                    requestId = java.util.UUID.randomUUID().toString(),
                    phraseText = phrase,
                    charStart = charStart,
                    charEnd = charEnd,
                    sourceTakeId = sourceTake,
                    alignmentStatus = "unverified",
                    rangeStartMs = null,
                    rangeEndMs = null,
                    replacementTakeId = null,
                    assemblyStatus = "none",
                    createdUtc = now,
                    updatedUtc = now
                )
                activeCreatorProject = store.save(
                    project.copy(phraseReplacementRequests = project.phraseReplacementRequests + req)
                )
                captionStatus.text =
                    "Saved phrase request (${phrase.take(40)}${if (phrase.length > 40) "\u2026" else ""}) · alignment unverified. No audio written."
            } catch (error: Exception) {
                captionStatus.text = "Could not save phrase request: ${error.message}"
            }
        }

'''
    if "fun savePhraseReplacementRequest" not in text:
        anchor = "        fun persistCaptions(next: List<CreatorProjectStore.Caption>)"
        if anchor not in text:
            anchor = "        fun linkTranscriptToSelectedTake"
        if anchor not in text:
            raise SystemExit("Cannot find captions helper insertion point")
        text = text.replace(anchor, helper + anchor, 1)

    if "Save phrase for replacement" not in text:
        block = '''
        creatorCaptionsPanel.addView(TextView(this).apply {
            text = "PHRASE REPLACEMENT"
            textSize = 11f
            setTypeface(null, android.graphics.Typeface.BOLD)
            setTextColor(Color.rgb(160, 190, 255))
            setPadding(0, dp(12), 0, dp(4))
        })
        creatorCaptionsPanel.addView(TextView(this).apply {
            text = "Select text in the transcript (or use the whole transcript), then save a request. Audio range and replacement recording come in later Phase 5 steps."
            textSize = 12f
            setTextColor(Color.rgb(174, 185, 204))
            setPadding(0, 0, 0, dp(6))
        })
        creatorCaptionsPanel.addView(exchangeButton("Save phrase for replacement", "Persist selection as an unverified phrase request") {
            savePhraseReplacementRequest()
        }, matchWidth())
'''
        for key in ('text="CAPTION DRAFT"', 'text = "CAPTION DRAFT"', 'TRANSCRIPT'):
            idx = text.find(key)
            if idx > 0:
                line_start = text.rfind("creatorCaptionsPanel.addView", 0, idx)
                if line_start > 0:
                    text = text[:line_start] + block + text[line_start:]
                    break
        else:
            raise SystemExit("Cannot find captions panel insertion point")

    path.write_text(text, encoding="utf-8")
    print("Android phrase replacement UI wired")


def ensure_windows() -> None:
    path = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml.cs"
    text = path.read_text(encoding="utf-8")
    if "SavePhraseReplacementRequest_Click" in text:
        print("Windows phrase replacement UI already present")
        return

    methods = r'''
    private void SavePhraseReplacementRequest_Click(object sender, RoutedEventArgs e)
    {
        if (_creatorProject is null || _creatorStore is null)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "No Creator project is loaded.";
            return;
        }
        try
        {
            var box = CreatorTranscriptBox;
            var full = box?.Text ?? _creatorProject.Transcript ?? string.Empty;
            string phrase;
            int? charStart = null;
            int? charEnd = null;
            if (box is not null && box.SelectionLength > 0)
            {
                charStart = box.SelectionStart;
                charEnd = box.SelectionStart + box.SelectionLength;
                phrase = full.Substring(charStart.Value, box.SelectionLength).Trim();
            }
            else
            {
                phrase = full.Trim();
            }
            if (string.IsNullOrWhiteSpace(phrase))
            {
                if (CreatorCaptionStatusText is not null)
                    CreatorCaptionStatusText.Text = "Select phrase text in the transcript, or enter a transcript first.";
                return;
            }
            var now = DateTime.UtcNow.ToString("O");
            var sourceTake = _creatorProject.Takes?.FirstOrDefault()?.TakeId;
            var req = new CreatorProjectStore.PhraseReplacementRequestDocument
            {
                RequestId = Guid.NewGuid().ToString(),
                PhraseText = phrase,
                CharStart = charStart,
                CharEnd = charEnd,
                SourceTakeId = sourceTake,
                AlignmentStatus = "unverified",
                AssemblyStatus = "none",
                CreatedUtc = now,
                UpdatedUtc = now,
            };
            _creatorProject.PhraseReplacementRequests ??= new List<CreatorProjectStore.PhraseReplacementRequestDocument>();
            _creatorProject.PhraseReplacementRequests.Add(req);
            _creatorStore.Save(_creatorProject);
            var preview = phrase.Length > 40 ? phrase[..40] + "\u2026" : phrase;
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text =
                    $"Saved phrase request ({preview}) · alignment unverified. No audio written.";
        }
        catch (Exception error)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Could not save phrase request: " + error.Message;
        }
    }

'''
    anchor = "    private void CreatorCaptionGenerate_Click"
    if anchor not in text:
        anchor = "    private void LinkTranscriptToFirstTake_Click"
    if anchor not in text:
        raise SystemExit("Cannot find Windows captions insertion point")
    text = text.replace(anchor, methods + "\n    " + anchor.lstrip(), 1)

    path.write_text(text, encoding="utf-8")
    print("Windows phrase replacement handler wired")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_phrase_replacement_ui:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
