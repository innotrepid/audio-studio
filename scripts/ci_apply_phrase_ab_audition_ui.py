#!/usr/bin/env python3
"""Wire Captions A/B audition for phrase original range vs replacement take."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
    text = path.read_text(encoding="utf-8")
    if "Play phrase original range" in text and "fun playPhraseOriginalRange" in text:
        print("Android phrase A/B audition already present")
        return

    helper = '''
        fun resolveTakeLocalPath(take: org.json.JSONObject): String? {
            val path = take.optString("sourcePath").takeIf { it.isNotBlank() && it != "null" }
            if (path != null && java.io.File(path).isFile) return path
            val uriText = take.optString("sourceUri").takeIf { it.isNotBlank() && it != "null" } ?: return null
            return try {
                val uri = android.net.Uri.parse(uriText)
                val tmp = java.io.File(cacheDir, "phrase-audition-${take.optString("takeId").take(8)}.wav")
                contentResolver.openInputStream(uri)?.use { input ->
                    tmp.outputStream().use { output -> input.copyTo(output) }
                } ?: return null
                if (tmp.isFile && tmp.length() > 44L) tmp.absolutePath else null
            } catch (_: Exception) {
                null
            }
        }

        fun activePhraseForAudition(): CreatorProjectStore.PhraseReplacementRequest? {
            val requests = activeCreatorProject?.phraseReplacementRequests.orEmpty()
            return requests.lastOrNull {
                it.alignmentStatus == "manual" &&
                    it.rangeStartMs != null && it.rangeEndMs != null &&
                    !it.replacementTakeId.isNullOrBlank()
            } ?: requests.lastOrNull {
                it.alignmentStatus == "manual" && it.rangeStartMs != null && it.rangeEndMs != null
            }
        }

        fun playPhraseOriginalRange() {
            val project = activeCreatorProject ?: return
            val req = activePhraseForAudition()
            if (req == null) {
                captionStatus.text = "Need a manual phrase with a confirmed range."
                return
            }
            val startMs = req.rangeStartMs ?: return
            val endMs = req.rangeEndMs ?: return
            if (endMs <= startMs) {
                captionStatus.text = "Phrase range is invalid."
                return
            }
            val sourceId = req.sourceTakeId
            val take = project.takes.lastOrNull { it.optString("takeId") == sourceId }
                ?: project.takes.lastOrNull()
            if (take == null) {
                captionStatus.text = "No source take available to play."
                return
            }
            val local = resolveTakeLocalPath(take)
            if (local == null) {
                captionStatus.text = "Could not open source take audio for audition."
                return
            }
            stopComparisonPlayback()
            try {
                val player = MediaPlayer()
                playback = player
                player.setDataSource(local)
                player.setOnPreparedListener { prepared ->
                    val safeStart = startMs.toInt().coerceAtLeast(0)
                    val safeEnd = endMs.toInt().coerceAtLeast(safeStart + 1)
                    prepared.seekTo(safeStart)
                    prepared.start()
                    stopPlaybackButton.isEnabled = true
                    captionStatus.text = "Playing original phrase range ${startMs}\u2013${endMs} ms."
                    mainHandler.postDelayed({
                        if (playback === prepared && prepared.isPlaying) {
                            try { prepared.pause() } catch (_: Exception) {}
                            stopComparisonPlayback()
                            captionStatus.text = "Original phrase range finished."
                        }
                    }, (safeEnd - safeStart).toLong().coerceAtLeast(50L))
                }
                player.setOnErrorListener { failed, _, _ ->
                    if (playback === failed) stopComparisonPlayback()
                    captionStatus.text = "Original range playback failed."
                    true
                }
                player.prepareAsync()
            } catch (error: Exception) {
                stopComparisonPlayback()
                captionStatus.text = "Could not play original range: ${error.message}"
            }
        }

        fun playPhraseReplacementTake() {
            val project = activeCreatorProject ?: return
            val req = activePhraseForAudition()
            if (req == null || req.replacementTakeId.isNullOrBlank()) {
                captionStatus.text = "Link a replacement take first."
                return
            }
            val take = project.takes.lastOrNull { it.optString("takeId") == req.replacementTakeId }
            if (take == null) {
                captionStatus.text = "Replacement take is missing from the project."
                return
            }
            val local = resolveTakeLocalPath(take)
            if (local == null) {
                captionStatus.text = "Could not open replacement take audio."
                return
            }
            stopComparisonPlayback()
            try {
                val player = MediaPlayer()
                playback = player
                player.setDataSource(local)
                player.setOnPreparedListener { prepared ->
                    prepared.start()
                    stopPlaybackButton.isEnabled = true
                    captionStatus.text = "Playing replacement take."
                }
                player.setOnCompletionListener { completed ->
                    if (playback === completed) {
                        stopComparisonPlayback()
                        captionStatus.text = "Replacement take finished."
                    }
                }
                player.setOnErrorListener { failed, _, _ ->
                    if (playback === failed) stopComparisonPlayback()
                    captionStatus.text = "Replacement playback failed."
                    true
                }
                player.prepareAsync()
            } catch (error: Exception) {
                stopComparisonPlayback()
                captionStatus.text = "Could not play replacement: ${error.message}"
            }
        }

'''
    if "fun playPhraseOriginalRange" not in text:
        anchor = "        fun linkLatestTakeAsPhraseReplacement()"
        if anchor not in text:
            anchor = "        fun confirmPhraseManualRange()"
        if anchor not in text:
            raise SystemExit("Cannot find phrase helpers for A/B insert")
        text = text.replace(anchor, helper + anchor, 1)

    if "Play phrase original range" not in text:
        block = '''
        creatorCaptionsPanel.addView(TextView(this).apply {
            text = "A/B AUDITION"
            textSize = 11f
            setTypeface(null, android.graphics.Typeface.BOLD)
            setTextColor(Color.rgb(160, 190, 255))
            setPadding(0, dp(12), 0, dp(4))
        })
        creatorCaptionsPanel.addView(TextView(this).apply {
            text = "Compare the source phrase range with the linked replacement take. Uses existing MediaPlayer; no files are modified."
            textSize = 12f
            setTextColor(Color.rgb(174, 185, 204))
            setPadding(0, 0, 0, dp(6))
        })
        creatorCaptionsPanel.addView(exchangeButton("Play phrase original range", "Play source take from rangeStartMs to rangeEndMs") {
            playPhraseOriginalRange()
        }, matchWidth())
        creatorCaptionsPanel.addView(exchangeButton("Play phrase replacement take", "Play the linked replacement take") {
            playPhraseReplacementTake()
        }, matchWidth())
        creatorCaptionsPanel.addView(exchangeButton("Stop phrase audition", "Stop MediaPlayer") {
            stopComparisonPlayback()
            captionStatus.text = "Phrase audition stopped."
        }, matchWidth())
'''
        key = "Link latest take as phrase replacement"
        idx = text.find(key)
        if idx < 0:
            key = "Confirm phrase range (manual)"
            idx = text.find(key)
        if idx < 0:
            raise SystemExit("Cannot find phrase UI for A/B panel")
        close = text.find("matchWidth())", idx)
        if close < 0:
            raise SystemExit("Cannot find button end for A/B insert")
        insert_at = close + len("matchWidth())")
        text = text[:insert_at] + "\n" + block + text[insert_at:]

    path.write_text(text, encoding="utf-8")
    print("Android phrase A/B audition wired")


def ensure_windows() -> None:
    path = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml.cs"
    text = path.read_text(encoding="utf-8")
    if "PlayPhraseOriginalRange_Click" in text:
        print("Windows phrase A/B audition already present")
        return

    methods = r'''
    private CreatorProjectStore.PhraseReplacementRequestDocument? ActivePhraseForAudition()
    {
        var requests = _creatorProject?.PhraseReplacementRequests;
        if (requests is null || requests.Count == 0) return null;
        return requests.LastOrDefault(r =>
                   r.AlignmentStatus == "manual" &&
                   r.RangeStartMs is not null && r.RangeEndMs is not null &&
                   !string.IsNullOrWhiteSpace(r.ReplacementTakeId))
               ?? requests.LastOrDefault(r =>
                   r.AlignmentStatus == "manual" &&
                   r.RangeStartMs is not null && r.RangeEndMs is not null);
    }

    private void StopPhraseAudition()
    {
        try
        {
            _previewPlayer.Pause();
            _previewPlayer.Position = TimeSpan.Zero;
        }
        catch { /* ignore */ }
    }

    private void PlayPhraseOriginalRange_Click(object sender, RoutedEventArgs e)
    {
        if (_creatorProject is null)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "No Creator project is loaded.";
            return;
        }
        var req = ActivePhraseForAudition();
        if (req is null || req.RangeStartMs is null || req.RangeEndMs is null)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Need a manual phrase with a confirmed range.";
            return;
        }
        var startMs = req.RangeStartMs.Value;
        var endMs = req.RangeEndMs.Value;
        if (endMs <= startMs)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Phrase range is invalid.";
            return;
        }
        var take = _creatorProject.Takes?.LastOrDefault(t => t.TakeId == req.SourceTakeId)
                   ?? _creatorProject.Takes?.LastOrDefault();
        if (take is null || string.IsNullOrWhiteSpace(take.SourcePath) || !File.Exists(take.SourcePath))
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Could not open source take audio for audition.";
            return;
        }
        try
        {
            StopPhraseAudition();
            _previewPlayer.SetSource(new Uri(take.SourcePath));
            _previewPlayer.MediaOpened += (_, _) =>
            {
                _previewPlayer.Position = TimeSpan.FromMilliseconds(startMs);
                _previewPlayer.Play();
                if (CreatorCaptionStatusText is not null)
                    CreatorCaptionStatusText.Text = $"Playing original phrase range {startMs}\u2013{endMs} ms.";
                var duration = endMs - startMs;
                _ = DispatcherQueue.TryEnqueue(async () =>
                {
                    await Task.Delay((int)Math.Max(50, duration));
                    StopPhraseAudition();
                    if (CreatorCaptionStatusText is not null)
                        CreatorCaptionStatusText.Text = "Original phrase range finished.";
                });
            };
            _previewPlayer.Play();
        }
        catch (Exception error)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Could not play original range: " + error.Message;
        }
    }

    private void PlayPhraseReplacementTake_Click(object sender, RoutedEventArgs e)
    {
        if (_creatorProject is null)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "No Creator project is loaded.";
            return;
        }
        var req = ActivePhraseForAudition();
        if (req is null || string.IsNullOrWhiteSpace(req.ReplacementTakeId))
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Link a replacement take first.";
            return;
        }
        var take = _creatorProject.Takes?.LastOrDefault(t => t.TakeId == req.ReplacementTakeId);
        if (take is null || string.IsNullOrWhiteSpace(take.SourcePath) || !File.Exists(take.SourcePath))
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Could not open replacement take audio.";
            return;
        }
        try
        {
            StopPhraseAudition();
            _previewPlayer.SetSource(new Uri(take.SourcePath));
            _previewPlayer.MediaEnded += (_, _) =>
            {
                if (CreatorCaptionStatusText is not null)
                    CreatorCaptionStatusText.Text = "Replacement take finished.";
            };
            _previewPlayer.Play();
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Playing replacement take.";
        }
        catch (Exception error)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Could not play replacement: " + error.Message;
        }
    }

    private void StopPhraseAudition_Click(object sender, RoutedEventArgs e)
    {
        StopPhraseAudition();
        if (CreatorCaptionStatusText is not null)
            CreatorCaptionStatusText.Text = "Phrase audition stopped.";
    }

'''
    anchor = "    private void LinkLatestTakeAsPhraseReplacement_Click"
    if anchor not in text:
        anchor = "    private void ConfirmPhraseManualRange_Click"
    if anchor not in text:
        raise SystemExit("Cannot find phrase handlers for A/B")
    text = text.replace(anchor, methods + "\n    " + anchor.lstrip(), 1)
    path.write_text(text, encoding="utf-8")
    print("Windows phrase A/B audition handlers wired")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_phrase_ab_audition_ui:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
