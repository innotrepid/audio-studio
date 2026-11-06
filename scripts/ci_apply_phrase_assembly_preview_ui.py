#!/usr/bin/env python3
"""Wire Captions phrase assembly preview (temp WAV, assemblyStatus=preview)."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
    text = path.read_text(encoding="utf-8")
    if "Build phrase assembly preview" in text and "fun buildPhraseAssemblyPreview" in text:
        print("Android phrase assembly preview already present")
        return

    helper = r'''
        fun concatPcm16WavFiles(parts: List<java.io.File>, output: java.io.File): Boolean {
            if (parts.isEmpty()) return false
            data class WavInfo(val sampleRate: Int, val channels: Int, val bits: Int, val dataOffset: Int, val dataSize: Int)
            fun parse(file: java.io.File): WavInfo? {
                val bytes = file.readBytes()
                if (bytes.size < 44) return null
                if (String(bytes, 0, 4) != "RIFF" || String(bytes, 8, 4) != "WAVE") return null
                var offset = 12
                var sampleRate = 0
                var channels = 0
                var bits = 16
                var dataOffset = -1
                var dataSize = 0
                while (offset + 8 <= bytes.size) {
                    val id = String(bytes, offset, 4)
                    val size = (bytes[offset + 4].toInt() and 0xff) or
                        ((bytes[offset + 5].toInt() and 0xff) shl 8) or
                        ((bytes[offset + 6].toInt() and 0xff) shl 16) or
                        ((bytes[offset + 7].toInt() and 0xff) shl 24)
                    if (id == "fmt " && offset + 24 <= bytes.size) {
                        channels = (bytes[offset + 10].toInt() and 0xff) or ((bytes[offset + 11].toInt() and 0xff) shl 8)
                        sampleRate = (bytes[offset + 12].toInt() and 0xff) or
                            ((bytes[offset + 13].toInt() and 0xff) shl 8) or
                            ((bytes[offset + 14].toInt() and 0xff) shl 16) or
                            ((bytes[offset + 15].toInt() and 0xff) shl 24)
                        bits = (bytes[offset + 22].toInt() and 0xff) or ((bytes[offset + 23].toInt() and 0xff) shl 8)
                    } else if (id == "data") {
                        dataOffset = offset + 8
                        dataSize = size.coerceAtMost(bytes.size - dataOffset)
                        break
                    }
                    offset += 8 + size + (size and 1)
                }
                if (dataOffset < 0 || sampleRate <= 0 || channels <= 0) return null
                return WavInfo(sampleRate, channels, bits, dataOffset, dataSize)
            }
            val infos = parts.map { parse(it) ?: return false }
            val sr = infos[0].sampleRate
            val ch = infos[0].channels
            val bits = infos[0].bits
            if (bits != 16) return false
            if (infos.any { it.sampleRate != sr || it.channels != ch || it.bits != bits }) return false
            val totalData = infos.sumOf { it.dataSize }
            val out = java.io.ByteArrayOutputStream()
            fun le16(v: Int) { out.write(v and 0xff); out.write((v shr 8) and 0xff) }
            fun le32(v: Int) {
                out.write(v and 0xff); out.write((v shr 8) and 0xff)
                out.write((v shr 16) and 0xff); out.write((v shr 24) and 0xff)
            }
            out.write("RIFF".toByteArray())
            le32(36 + totalData)
            out.write("WAVE".toByteArray())
            out.write("fmt ".toByteArray())
            le32(16); le16(1); le16(ch); le32(sr)
            le32(sr * ch * 2); le16(ch * 2); le16(16)
            out.write("data".toByteArray())
            le32(totalData)
            parts.forEachIndexed { index, file ->
                val bytes = file.readBytes()
                val info = infos[index]
                out.write(bytes, info.dataOffset, info.dataSize)
            }
            output.writeBytes(out.toByteArray())
            return output.isFile && output.length() > 44L
        }

        fun buildPhraseAssemblyPreview() {
            val store = creatorProjectStore ?: return
            val project = activeCreatorProject ?: return
            val req = project.phraseReplacementRequests.lastOrNull {
                it.alignmentStatus == "manual" &&
                    it.rangeStartMs != null && it.rangeEndMs != null &&
                    !it.replacementTakeId.isNullOrBlank()
            }
            if (req == null) {
                captionStatus.text = "Need a manual phrase with range + linked replacement take."
                return
            }
            val startMs = req.rangeStartMs!!
            val endMs = req.rangeEndMs!!
            if (endMs <= startMs) {
                captionStatus.text = "Phrase range is invalid."
                return
            }
            val sourceTake = project.takes.lastOrNull { it.optString("takeId") == req.sourceTakeId }
                ?: project.takes.lastOrNull()
            val replTake = project.takes.lastOrNull { it.optString("takeId") == req.replacementTakeId }
            if (sourceTake == null || replTake == null) {
                captionStatus.text = "Source or replacement take is missing."
                return
            }
            val sourcePath = resolveTakeLocalPath(sourceTake)
            val replPath = resolveTakeLocalPath(replTake)
            if (sourcePath == null || replPath == null) {
                captionStatus.text = "Could not open take audio for assembly."
                return
            }
            try {
                val meta = nativeGetWavMetadata(sourcePath)
                if (meta.size < 3 || meta[1] <= 0L) {
                    captionStatus.text = "Source take metadata unavailable."
                    return
                }
                val sampleRate = meta[1]
                val totalFrames = meta[0]
                val startFrame = (startMs * sampleRate / 1000L).coerceIn(0L, totalFrames)
                val endFrame = (endMs * sampleRate / 1000L).coerceIn(startFrame, totalFrames)
                val dir = java.io.File(cacheDir, "phrase-assembly").apply { mkdirs() }
                val head = java.io.File(dir, "head-${req.requestId.take(8)}.wav")
                val tail = java.io.File(dir, "tail-${req.requestId.take(8)}.wav")
                val preview = java.io.File(dir, "preview-${req.requestId.take(8)}.wav")
                val parts = mutableListOf<java.io.File>()
                if (startFrame > 0L) {
                    val headStatus = nativeTrimWav(sourcePath, head.absolutePath, 0L, startFrame)
                    if (headStatus.isNotBlank() && !head.isFile) {
                        captionStatus.text = "Head trim failed: $headStatus"
                        return
                    }
                    if (head.isFile) parts.add(head)
                }
                parts.add(java.io.File(replPath))
                if (endFrame < totalFrames) {
                    val tailStatus = nativeTrimWav(sourcePath, tail.absolutePath, endFrame, totalFrames)
                    if (tailStatus.isNotBlank() && !tail.isFile) {
                        captionStatus.text = "Tail trim failed: $tailStatus"
                        return
                    }
                    if (tail.isFile) parts.add(tail)
                }
                if (!concatPcm16WavFiles(parts, preview)) {
                    captionStatus.text = "Could not concatenate preview WAV (sample rate/channel mismatch?)."
                    return
                }
                val now = java.time.Instant.now().toString()
                val updated = project.phraseReplacementRequests.map {
                    if (it.requestId == req.requestId) it.copy(assemblyStatus = "preview", updatedUtc = now) else it
                }
                activeCreatorProject = store.save(project.copy(phraseReplacementRequests = updated))
                stopComparisonPlayback()
                val player = MediaPlayer()
                playback = player
                player.setDataSource(preview.absolutePath)
                player.setOnPreparedListener { prepared ->
                    prepared.start()
                    stopPlaybackButton.isEnabled = true
                    captionStatus.text =
                        "Assembly preview playing (head+replacement+tail). assemblyStatus=preview. Source takes unchanged."
                }
                player.setOnCompletionListener {
                    if (playback === it) stopComparisonPlayback()
                    captionStatus.text = "Assembly preview finished. Approve step comes next."
                }
                player.setOnErrorListener { failed, _, _ ->
                    if (playback === failed) stopComparisonPlayback()
                    captionStatus.text = "Assembly preview playback failed."
                    true
                }
                player.prepareAsync()
            } catch (error: Exception) {
                captionStatus.text = "Assembly preview failed: ${error.message}"
            }
        }

'''
    if "fun buildPhraseAssemblyPreview" not in text:
        anchor = "        fun playPhraseOriginalRange()"
        if anchor not in text:
            anchor = "        fun linkLatestTakeAsPhraseReplacement()"
        if anchor not in text:
            raise SystemExit("Cannot find phrase helpers for assembly insert")
        text = text.replace(anchor, helper + anchor, 1)

    if "Build phrase assembly preview" not in text:
        block = '''
        creatorCaptionsPanel.addView(TextView(this).apply {
            text = "ASSEMBLY PREVIEW"
            textSize = 11f
            setTypeface(null, android.graphics.Typeface.BOLD)
            setTextColor(Color.rgb(160, 190, 255))
            setPadding(0, dp(12), 0, dp(4))
        })
        creatorCaptionsPanel.addView(TextView(this).apply {
            text = "Build a temporary head+replacement+tail WAV and play it. Source takes stay unchanged. Crossfade is deferred."
            textSize = 12f
            setTextColor(Color.rgb(174, 185, 204))
            setPadding(0, 0, 0, dp(6))
        })
        creatorCaptionsPanel.addView(exchangeButton("Build phrase assembly preview", "Temp WAV via trim+concat; assemblyStatus=preview") {
            buildPhraseAssemblyPreview()
        }, matchWidth())
'''
        key = "Play phrase replacement take"
        idx = text.find(key)
        if idx < 0:
            key = "Link latest take as phrase replacement"
            idx = text.find(key)
        if idx < 0:
            raise SystemExit("Cannot find A/B UI for assembly panel")
        close = text.find("matchWidth())", idx)
        if close < 0:
            raise SystemExit("Cannot find button end for assembly")
        stop_idx = text.find("Stop phrase audition", idx)
        if stop_idx > 0:
            close = text.find("matchWidth())", stop_idx)
        insert_at = close + len("matchWidth())")
        text = text[:insert_at] + "\n" + block + text[insert_at:]

    path.write_text(text, encoding="utf-8")
    print("Android phrase assembly preview wired")


def ensure_windows() -> None:
    path = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml.cs"
    text = path.read_text(encoding="utf-8")
    # Heal ulong -> long assignment from earlier apply
    if "totalFrames = _waveformTotalFrames;" in text:
        text = text.replace(
            "totalFrames = _waveformTotalFrames;",
            "totalFrames = (long)Math.Min(_waveformTotalFrames, (ulong)long.MaxValue);",
            1,
        )
        path.write_text(text, encoding="utf-8")
        print("Healed totalFrames ulong cast")
        text = path.read_text(encoding="utf-8")
    if "BuildPhraseAssemblyPreview_Click" in text:
        print("Windows phrase assembly preview already present")
        return

    methods = r'''
    private static bool ConcatPcm16WavFiles(IReadOnlyList<string> parts, string outputPath)
    {
        if (parts.Count == 0) return false;

        static (int sampleRate, int channels, int bits, int dataOffset, int dataSize)? Parse(string path)
        {
            var bytes = File.ReadAllBytes(path);
            if (bytes.Length < 44) return null;
            if (System.Text.Encoding.ASCII.GetString(bytes, 0, 4) != "RIFF" ||
                System.Text.Encoding.ASCII.GetString(bytes, 8, 4) != "WAVE")
                return null;
            var offset = 12;
            var sampleRate = 0;
            var channels = 0;
            var bits = 16;
            var dataOffset = -1;
            var dataSize = 0;
            while (offset + 8 <= bytes.Length)
            {
                var id = System.Text.Encoding.ASCII.GetString(bytes, offset, 4);
                var size = BitConverter.ToInt32(bytes, offset + 4);
                if (id == "fmt " && offset + 24 <= bytes.Length)
                {
                    channels = BitConverter.ToInt16(bytes, offset + 10);
                    sampleRate = BitConverter.ToInt32(bytes, offset + 12);
                    bits = BitConverter.ToInt16(bytes, offset + 22);
                }
                else if (id == "data")
                {
                    dataOffset = offset + 8;
                    dataSize = Math.Min(size, bytes.Length - dataOffset);
                    break;
                }
                offset += 8 + size + (size & 1);
            }
            if (dataOffset < 0 || sampleRate <= 0 || channels <= 0) return null;
            return (sampleRate, channels, bits, dataOffset, dataSize);
        }

        var infos = new List<(int sampleRate, int channels, int bits, int dataOffset, int dataSize)>();
        foreach (var p in parts)
        {
            var info = Parse(p);
            if (info is null) return false;
            infos.Add(info.Value);
        }
        var sr = infos[0].sampleRate;
        var ch = infos[0].channels;
        var bits = infos[0].bits;
        if (bits != 16) return false;
        if (infos.Any(i => i.sampleRate != sr || i.channels != ch || i.bits != bits)) return false;
        var totalData = infos.Sum(i => i.dataSize);
        using var ms = new MemoryStream();
        void Le16(int v) { ms.WriteByte((byte)(v & 0xff)); ms.WriteByte((byte)((v >> 8) & 0xff)); }
        void Le32(int v)
        {
            ms.WriteByte((byte)(v & 0xff));
            ms.WriteByte((byte)((v >> 8) & 0xff));
            ms.WriteByte((byte)((v >> 16) & 0xff));
            ms.WriteByte((byte)((v >> 24) & 0xff));
        }
        var ascii = System.Text.Encoding.ASCII;
        ms.Write(ascii.GetBytes("RIFF"));
        Le32(36 + totalData);
        ms.Write(ascii.GetBytes("WAVE"));
        ms.Write(ascii.GetBytes("fmt "));
        Le32(16); Le16(1); Le16(ch); Le32(sr);
        Le32(sr * ch * 2); Le16(ch * 2); Le16(16);
        ms.Write(ascii.GetBytes("data"));
        Le32(totalData);
        for (var i = 0; i < parts.Count; i++)
        {
            var bytes = File.ReadAllBytes(parts[i]);
            var info = infos[i];
            ms.Write(bytes, info.dataOffset, info.dataSize);
        }
        File.WriteAllBytes(outputPath, ms.ToArray());
        return File.Exists(outputPath) && new FileInfo(outputPath).Length > 44;
    }

    private async void BuildPhraseAssemblyPreview_Click(object sender, RoutedEventArgs e)
    {
        if (_creatorProject is null || _creatorStore is null)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "No Creator project is loaded.";
            return;
        }
        var req = _creatorProject.PhraseReplacementRequests?.LastOrDefault(r =>
            r.AlignmentStatus == "manual" &&
            r.RangeStartMs is not null && r.RangeEndMs is not null &&
            !string.IsNullOrWhiteSpace(r.ReplacementTakeId));
        if (req is null)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Need a manual phrase with range + linked replacement take.";
            return;
        }
        var startMs = req.RangeStartMs!.Value;
        var endMs = req.RangeEndMs!.Value;
        if (endMs <= startMs)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Phrase range is invalid.";
            return;
        }
        var sourceTake = _creatorProject.Takes?.LastOrDefault(t => t.TakeId == req.SourceTakeId)
                         ?? _creatorProject.Takes?.LastOrDefault();
        var replTake = _creatorProject.Takes?.LastOrDefault(t => t.TakeId == req.ReplacementTakeId);
        if (sourceTake is null || replTake is null ||
            string.IsNullOrWhiteSpace(sourceTake.SourcePath) || !File.Exists(sourceTake.SourcePath) ||
            string.IsNullOrWhiteSpace(replTake.SourcePath) || !File.Exists(replTake.SourcePath))
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Could not open take audio for assembly.";
            return;
        }
        try
        {
            var dir = Path.Combine(Path.GetTempPath(), "synvidra-phrase-assembly");
            Directory.CreateDirectory(dir);
            var head = Path.Combine(dir, $"head-{req.RequestId[..Math.Min(8, req.RequestId.Length)]}.wav");
            var tail = Path.Combine(dir, $"tail-{req.RequestId[..Math.Min(8, req.RequestId.Length)]}.wav");
            var preview = Path.Combine(dir, $"preview-{req.RequestId[..Math.Min(8, req.RequestId.Length)]}.wav");

            long sampleRate = 48000;
            long totalFrames = 0;
            if (!string.IsNullOrWhiteSpace(_waveformFilePath) &&
                string.Equals(_waveformFilePath, sourceTake.SourcePath, StringComparison.OrdinalIgnoreCase) &&
                _waveformSampleRate > 0 && _waveformTotalFrames > 1)
            {
                sampleRate = _waveformSampleRate;
                totalFrames = (long)Math.Min(_waveformTotalFrames, (ulong)long.MaxValue);
            }
            else if (sourceTake.SampleRateHz > 0 && sourceTake.DurationMs > 0)
            {
                sampleRate = sourceTake.SampleRateHz;
                totalFrames = (long)Math.Round(sourceTake.DurationMs * (double)sampleRate / 1000.0);
            }

            if (sampleRate <= 0 || totalFrames <= 0)
            {
                if (CreatorCaptionStatusText is not null)
                    CreatorCaptionStatusText.Text = "Source take duration/sample rate unknown; open it in Editor first.";
                return;
            }

            var startFrame = Math.Clamp(startMs * sampleRate / 1000L, 0L, totalFrames);
            var endFrame = Math.Clamp(endMs * sampleRate / 1000L, startFrame, totalFrames);
            var parts = new List<string>();

            if (startFrame > 0)
            {
                var trimHead = NativeTrimWav(sourceTake.SourcePath, head, 0UL, (ulong)startFrame);
                if (trimHead == 0 || !File.Exists(head))
                {
                    if (CreatorCaptionStatusText is not null)
                        CreatorCaptionStatusText.Text = "Head trim failed.";
                    return;
                }
                parts.Add(head);
            }
            parts.Add(replTake.SourcePath);
            if (endFrame < totalFrames)
            {
                var trimTail = NativeTrimWav(sourceTake.SourcePath, tail, (ulong)endFrame, (ulong)totalFrames);
                if (trimTail == 0 || !File.Exists(tail))
                {
                    if (CreatorCaptionStatusText is not null)
                        CreatorCaptionStatusText.Text = "Tail trim failed.";
                    return;
                }
                parts.Add(tail);
            }

            if (!ConcatPcm16WavFiles(parts, preview))
            {
                if (CreatorCaptionStatusText is not null)
                    CreatorCaptionStatusText.Text = "Could not concatenate preview WAV (sample rate/channel mismatch?).";
                return;
            }

            req.AssemblyStatus = "preview";
            req.UpdatedUtc = DateTime.UtcNow.ToString("O");
            _creatorStore.Save(_creatorProject);

            StopPhraseAudition();
            var file = await StorageFile.GetFileFromPathAsync(preview);
            _previewPlayer.Source = MediaSource.CreateFromStorageFile(file);
            await Task.Delay(100);
            _previewPlayer.Play();
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text =
                    "Assembly preview playing (head+replacement+tail). assemblyStatus=preview. Source takes unchanged.";
        }
        catch (Exception error)
        {
            if (CreatorCaptionStatusText is not null)
                CreatorCaptionStatusText.Text = "Assembly preview failed: " + error.Message;
        }
    }

'''
    anchor = "    private void PlayPhraseOriginalRange_Click"
    if anchor not in text:
        anchor = "    private async void PlayPhraseOriginalRange_Click"
    if anchor not in text:
        anchor = "    private void LinkLatestTakeAsPhraseReplacement_Click"
    if anchor not in text:
        raise SystemExit("Cannot find A/B handlers for assembly")
    text = text.replace(anchor, methods + "\n    " + anchor.lstrip(), 1)
    path.write_text(text, encoding="utf-8")
    print("Windows phrase assembly preview handlers wired")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_phrase_assembly_preview_ui:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
