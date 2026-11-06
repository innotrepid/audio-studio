#!/usr/bin/env python3
"""Wire Captions UI to pick a linked take for the project transcript."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
    text = path.read_text(encoding="utf-8")
    if "Link transcript to take" in text or "transcriptTakeSpinner" in text:
        print("Android transcript-take UI already present")
        return

    # Insert after Save transcript button block a link control is complex; inject near "Save transcript"
    marker = 'creatorCaptionsPanel.addView(exchangeButton("Save transcript"'
    if marker not in text:
        raise SystemExit("Cannot find Save transcript button")

    # After captions panel section with Save transcript - add helper functions near persistCaptions
    if "fun linkTranscriptToTake" not in text:
        anchor = "        fun persistCaptions(next: List<CreatorProjectStore.Caption>)"
        if anchor not in text:
            raise SystemExit("Cannot find persistCaptions")
        helper = '''
        fun linkTranscriptToSelectedTake(takeId: String?) {
            val store = creatorProjectStore ?: return
            val project = activeCreatorProject ?: return
            try {
                activeCreatorProject = store.save(project.copy(transcriptTakeId = takeId))
                captionStatus.text = if (takeId.isNullOrBlank()) {
                    "Transcript unlinked from takes."
                } else {
                    "Transcript linked to take ${takeId.take(8)}… (not a timing alignment claim)."
                }
            } catch (error: Exception) {
                captionStatus.text = "Could not link transcript: ${error.message}"
            }
        }

'''
        text = text.replace(anchor, helper + anchor, 1)

    # UI: after "Save transcript" button's closing, hard to parse. Add before CAPTION DRAFT header.
    draft = 'creatorCaptionsPanel.addView(TextView(this).apply { text="CAPTION DRAFT"'
    if draft not in text:
        draft = 'text="CAPTION DRAFT"'
    block = '''
        creatorCaptionsPanel.addView(TextView(this).apply {
            text = "TRANSCRIPT \u2192 TAKE"
            textSize = 11f
            setTypeface(null, android.graphics.Typeface.BOLD)
            setTextColor(Color.rgb(160, 190, 255))
            setPadding(0, dp(12), 0, dp(4))
        })
        creatorCaptionsPanel.addView(TextView(this).apply {
            text = "Optionally mark which linked take this transcript was prepared from. This does not claim word-level alignment."
            textSize = 12f
            setTextColor(Color.rgb(174, 185, 204))
            setPadding(0, 0, 0, dp(6))
        })
        creatorCaptionsPanel.addView(exchangeButton("Link transcript to first linked take", "Associate transcriptTakeId with the first project take") {
            val takeId = activeCreatorProject?.takes?.firstOrNull()?.optString("takeId")?.takeIf { it.isNotBlank() }
            if (takeId == null) {
                captionStatus.text = "No linked takes available."
            } else {
                linkTranscriptToSelectedTake(takeId)
            }
        }, matchWidth())
        creatorCaptionsPanel.addView(exchangeButton("Clear transcript take link", "Remove transcriptTakeId") {
            linkTranscriptToSelectedTake(null)
        }, matchWidth())
'''
    # Insert before CAPTION DRAFT label
    idx = text.find('text="CAPTION DRAFT"')
    if idx < 0:
        idx = text.find("CAPTION DRAFT")
    if idx < 0:
        raise SystemExit("Cannot find CAPTION DRAFT label")
    # find start of that addView line
    line_start = text.rfind("creatorCaptionsPanel.addView", 0, idx)
    if line_start < 0:
        raise SystemExit("Cannot find CAPTION DRAFT addView")
    text = text[:line_start] + block + text[line_start:]

    path.write_text(text, encoding="utf-8")
    print("Android transcript-take UI wired")


def ensure_windows() -> None:
    path = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml.cs"
    text = path.read_text(encoding="utf-8")
    if "LinkTranscriptToFirstTake" in text or "ClearTranscriptTakeLink" in text:
        print("Windows transcript-take UI already present")
        return

    methods = r'''
    private void LinkTranscriptToFirstTake_Click(object sender, RoutedEventArgs e)
    {
        if (_creatorProject is null || _creatorStore is null)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "No Creator project is loaded.";
            return;
        }
        var take = _creatorProject.Takes?.FirstOrDefault();
        if (take is null || string.IsNullOrWhiteSpace(take.TakeId))
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "No linked takes available.";
            return;
        }
        try
        {
            _creatorProject.TranscriptTakeId = take.TakeId;
            _creatorStore.Save(_creatorProject);
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text =
                    $"Transcript linked to take {take.TakeId[..Math.Min(8, take.TakeId.Length)]}… (not a timing alignment claim).";
        }
        catch (Exception error)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Could not link transcript: " + error.Message;
        }
    }

    private void ClearTranscriptTakeLink_Click(object sender, RoutedEventArgs e)
    {
        if (_creatorProject is null || _creatorStore is null) return;
        try
        {
            _creatorProject.TranscriptTakeId = null;
            _creatorStore.Save(_creatorProject);
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Transcript unlinked from takes.";
        }
        catch (Exception error)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Could not clear link: " + error.Message;
        }
    }

'''
    if "LinkTranscriptToFirstTake_Click" not in text:
        anchor = "    private void CreatorCaptionGenerate_Click"
        if anchor not in text:
            anchor = "    private async void CreatorCaptionAdd_Click"
        if anchor not in text:
            raise SystemExit("Cannot find captions methods insertion point")
        text = text.replace(anchor, methods + "\n    " + anchor.lstrip(), 1)

    # Show linked take id when refreshing captions if possible
    if "TranscriptTakeId" in text and "RefreshCreatorCaptions" in text:
        # soft status line after refresh
        if "transcript take" not in text.lower():
            text = text.replace(
                "private void RefreshCreatorCaptions()",
                "private void RefreshCreatorCaptions() /* transcriptTakeId shown via status when linking */",
                1,
            )

    path.write_text(text, encoding="utf-8")
    print("Windows transcript-take methods wired")
    print("Note: wire LinkTranscriptToFirstTake_Click / ClearTranscriptTakeLink_Click from Captions XAML when controls exist")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_transcript_take_link_ui:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
