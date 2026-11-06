#!/usr/bin/env python3
"""Wire UI to set manual audio range on a phrase replacement request."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

DECLS = '''
        val phraseRangeStartInput = EditText(this).apply {
            hint = "Phrase range start (ms)"
            inputType = android.text.InputType.TYPE_CLASS_NUMBER
            setTextColor(Color.WHITE)
            setHintTextColor(Color.rgb(140, 150, 170))
        }
        val phraseRangeEndInput = EditText(this).apply {
            hint = "Phrase range end (ms)"
            inputType = android.text.InputType.TYPE_CLASS_NUMBER
            setTextColor(Color.WHITE)
            setHintTextColor(Color.rgb(140, 150, 170))
        }
'''

HELPER = '''
        fun confirmPhraseManualRange() {
            val store = creatorProjectStore ?: return
            val project = activeCreatorProject ?: return
            val requests = project.phraseReplacementRequests
            if (requests.isEmpty()) {
                captionStatus.text = "Save a phrase request first."
                return
            }
            val startMs = phraseRangeStartInput.text?.toString()?.trim()?.toLongOrNull()
            val endMs = phraseRangeEndInput.text?.toString()?.trim()?.toLongOrNull()
            if (startMs == null || endMs == null || startMs < 0 || endMs <= startMs) {
                captionStatus.text = "Enter valid start/end milliseconds (end must be greater than start)."
                return
            }
            try {
                val now = java.time.Instant.now().toString()
                val targetId = requests.lastOrNull { it.alignmentStatus == "unverified" }?.requestId
                    ?: requests.last().requestId
                val updated = requests.map { req ->
                    if (req.requestId == targetId) {
                        req.copy(
                            rangeStartMs = startMs,
                            rangeEndMs = endMs,
                            alignmentStatus = "manual",
                            updatedUtc = now
                        )
                    } else req
                }
                activeCreatorProject = store.save(project.copy(phraseReplacementRequests = updated))
                captionStatus.text =
                    "Phrase range set ${startMs}\u2013${endMs} ms · alignmentStatus=manual. Still no audio write."
            } catch (error: Exception) {
                captionStatus.text = "Could not confirm range: ${error.message}"
            }
        }

'''

PANEL = '''
        creatorCaptionsPanel.addView(TextView(this).apply {
            text = "MANUAL AUDIO RANGE"
            textSize = 11f
            setTypeface(null, android.graphics.Typeface.BOLD)
            setTextColor(Color.rgb(160, 190, 255))
            setPadding(0, dp(12), 0, dp(4))
        })
        creatorCaptionsPanel.addView(TextView(this).apply {
            text = "Set start/end milliseconds for the latest unverified phrase (listen in Editor to choose times). Marks alignment as manual."
            textSize = 12f
            setTextColor(Color.rgb(174, 185, 204))
            setPadding(0, 0, 0, dp(6))
        })
        creatorCaptionsPanel.addView(phraseRangeStartInput, matchWidth())
        creatorCaptionsPanel.addView(phraseRangeEndInput, matchWidth())
        creatorCaptionsPanel.addView(exchangeButton("Confirm phrase range (manual)", "Set rangeStartMs/rangeEndMs and alignmentStatus=manual") {
            confirmPhraseManualRange()
        }, matchWidth())
'''


def ensure_android() -> None:
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
    text = path.read_text(encoding="utf-8")

    # Always ensure val declarations exist (helper text alone is not a declaration)
    if "val phraseRangeStartInput" not in text:
        decl_anchor = (
            "        val captionStatus = TextView(this).apply { "
            "textSize = 12f; setTextColor(Color.rgb(190, 198, 214)) }"
        )
        if decl_anchor not in text:
            raise SystemExit("Cannot find captionStatus declaration")
        text = text.replace(decl_anchor, decl_anchor + "\n" + DECLS, 1)
        print("Declared phraseRangeStartInput/EndInput")

    if "fun confirmPhraseManualRange" not in text:
        anchor = "        fun savePhraseReplacementRequest()"
        if anchor not in text:
            raise SystemExit("Cannot find savePhraseReplacementRequest — run phrase selection apply first")
        text = text.replace(anchor, HELPER + anchor, 1)
        print("Inserted confirmPhraseManualRange")

    if "Confirm phrase range (manual)" not in text:
        key = "Save phrase for replacement"
        idx = text.find(key)
        if idx < 0:
            raise SystemExit("Cannot find Save phrase for replacement UI")
        close = text.find("matchWidth())", idx)
        if close < 0:
            raise SystemExit("Cannot find end of Save phrase button")
        insert_at = close + len("matchWidth())")
        text = text[:insert_at] + "\n" + PANEL + text[insert_at:]
        print("Inserted manual range panel")

    path.write_text(text, encoding="utf-8")
    print("Android phrase manual range UI wired")


def ensure_windows() -> None:
    path = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml.cs"
    text = path.read_text(encoding="utf-8")
    if "ConfirmPhraseManualRange_Click" in text:
        print("Windows phrase manual range handler already present")
        return

    methods = r'''
    private void ConfirmPhraseManualRange_Click(object sender, RoutedEventArgs e)
    {
        if (_creatorProject is null || _creatorStore is null)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "No Creator project is loaded.";
            return;
        }
        var requests = _creatorProject.PhraseReplacementRequests;
        if (requests is null || requests.Count == 0)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Save a phrase request first.";
            return;
        }
        if (!long.TryParse(PhraseRangeStartBox?.Text?.Trim(), out var startMs) ||
            !long.TryParse(PhraseRangeEndBox?.Text?.Trim(), out var endMs) ||
            startMs < 0 || endMs <= startMs)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Enter valid start/end milliseconds (end must be greater than start).";
            return;
        }
        try
        {
            var target = requests.LastOrDefault(r => r.AlignmentStatus == "unverified") ?? requests[^1];
            target.RangeStartMs = startMs;
            target.RangeEndMs = endMs;
            target.AlignmentStatus = "manual";
            target.UpdatedUtc = DateTime.UtcNow.ToString("O");
            _creatorStore.Save(_creatorProject);
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text =
                    $"Phrase range set {startMs}\u2013{endMs} ms · alignmentStatus=manual. Still no audio write.";
        }
        catch (Exception error)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Could not confirm range: " + error.Message;
        }
    }

'''
    anchor = "    private void SavePhraseReplacementRequest_Click"
    if anchor not in text:
        raise SystemExit("Cannot find SavePhraseReplacementRequest_Click")
    text = text.replace(anchor, methods + "\n    " + anchor.lstrip(), 1)
    path.write_text(text, encoding="utf-8")
    print("Windows phrase manual range handler wired")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_phrase_manual_range_ui:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
