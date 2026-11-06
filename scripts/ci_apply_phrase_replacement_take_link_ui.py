#!/usr/bin/env python3
"""Link latest project take as replacementTakeId on a manual phrase request."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
    text = path.read_text(encoding="utf-8")
    if "Link latest take as phrase replacement" in text and "fun linkLatestTakeAsPhraseReplacement" in text:
        print("Android phrase replacement take link already present")
        return

    helper = '''
        fun linkLatestTakeAsPhraseReplacement() {
            val store = creatorProjectStore ?: return
            val project = activeCreatorProject ?: return
            val takeId = project.takes.lastOrNull()?.optString("takeId")?.takeIf { it.isNotBlank() }
            if (takeId == null) {
                captionStatus.text = "No linked takes yet. Record a studio take first."
                return
            }
            val requests = project.phraseReplacementRequests
            if (requests.isEmpty()) {
                captionStatus.text = "Save a phrase request and confirm its range first."
                return
            }
            val target = requests.lastOrNull {
                it.alignmentStatus == "manual" && it.replacementTakeId.isNullOrBlank()
            } ?: requests.lastOrNull { it.alignmentStatus == "manual" }
            if (target == null) {
                captionStatus.text = "Confirm a manual range on a phrase request before linking a replacement take."
                return
            }
            try {
                val now = java.time.Instant.now().toString()
                val updated = requests.map { req ->
                    if (req.requestId == target.requestId) {
                        req.copy(replacementTakeId = takeId, updatedUtc = now)
                    } else req
                }
                activeCreatorProject = store.save(project.copy(phraseReplacementRequests = updated))
                captionStatus.text =
                    "Linked take ${takeId.take(8)}\u2026 as replacement for phrase \u201c${target.phraseText.take(32)}\u201d. A/B and assembly come next."
            } catch (error: Exception) {
                captionStatus.text = "Could not link replacement take: ${error.message}"
            }
        }

'''
    if "fun linkLatestTakeAsPhraseReplacement" not in text:
        anchor = "        fun confirmPhraseManualRange()"
        if anchor not in text:
            anchor = "        fun savePhraseReplacementRequest()"
        if anchor not in text:
            raise SystemExit("Cannot find phrase helpers")
        text = text.replace(anchor, helper + anchor, 1)

    if "Link latest take as phrase replacement" not in text:
        block = '''
        creatorCaptionsPanel.addView(exchangeButton("Link latest take as phrase replacement", "Set replacementTakeId on latest manual phrase request") {
            linkLatestTakeAsPhraseReplacement()
        }, matchWidth())
'''
        key = "Confirm phrase range (manual)"
        idx = text.find(key)
        if idx < 0:
            key = "Save phrase for replacement"
            idx = text.find(key)
        if idx < 0:
            raise SystemExit("Cannot find phrase UI insertion point")
        close = text.find("matchWidth())", idx)
        if close < 0:
            raise SystemExit("Cannot find button end")
        insert_at = close + len("matchWidth())")
        text = text[:insert_at] + "\n" + block + text[insert_at:]

    path.write_text(text, encoding="utf-8")
    print("Android phrase replacement take link wired")


def ensure_windows() -> None:
    path = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml.cs"
    text = path.read_text(encoding="utf-8")
    if "LinkLatestTakeAsPhraseReplacement_Click" in text:
        print("Windows phrase replacement take link already present")
        return

    methods = r'''
    private void LinkLatestTakeAsPhraseReplacement_Click(object sender, RoutedEventArgs e)
    {
        if (_creatorProject is null || _creatorStore is null)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "No Creator project is loaded.";
            return;
        }
        var takeId = _creatorProject.Takes?.LastOrDefault()?.TakeId;
        if (string.IsNullOrWhiteSpace(takeId))
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "No linked takes yet. Record a studio take first.";
            return;
        }
        var requests = _creatorProject.PhraseReplacementRequests;
        if (requests is null || requests.Count == 0)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Save a phrase request and confirm its range first.";
            return;
        }
        var target = requests.LastOrDefault(r =>
                         r.AlignmentStatus == "manual" && string.IsNullOrWhiteSpace(r.ReplacementTakeId))
                     ?? requests.LastOrDefault(r => r.AlignmentStatus == "manual");
        if (target is null)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text =
                    "Confirm a manual range on a phrase request before linking a replacement take.";
            return;
        }
        try
        {
            target.ReplacementTakeId = takeId;
            target.UpdatedUtc = DateTime.UtcNow.ToString("O");
            _creatorStore.Save(_creatorProject);
            var preview = target.PhraseText.Length > 32 ? target.PhraseText[..32] + "\u2026" : target.PhraseText;
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text =
                    $"Linked take {takeId[..Math.Min(8, takeId.Length)]}\u2026 as replacement for phrase \u201c{preview}\u201d. A/B and assembly come next.";
        }
        catch (Exception error)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Could not link replacement take: " + error.Message;
        }
    }

'''
    anchor = "    private void ConfirmPhraseManualRange_Click"
    if anchor not in text:
        anchor = "    private void SavePhraseReplacementRequest_Click"
    if anchor not in text:
        raise SystemExit("Cannot find phrase click handlers")
    text = text.replace(anchor, methods + "\n    " + anchor.lstrip(), 1)
    path.write_text(text, encoding="utf-8")
    print("Windows phrase replacement take link handler wired")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_phrase_replacement_take_link_ui:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
