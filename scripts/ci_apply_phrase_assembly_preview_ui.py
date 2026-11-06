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
    raise SystemExit(
        "Android phrase assembly preview missing on base tree; re-apply from a complete script revision."
    )


def ensure_windows() -> None:
    path = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml.cs"
    text = path.read_text(encoding="utf-8")

    # Heal ulong -> long
    if "totalFrames = _waveformTotalFrames;" in text:
        text = text.replace(
            "totalFrames = _waveformTotalFrames;",
            "totalFrames = (long)Math.Min(_waveformTotalFrames, (ulong)long.MaxValue);",
            1,
        )
        print("Healed totalFrames ulong cast")

    # Heal nullable SampleRateHz / DurationMs assignments
    if "sampleRate = sourceTake.SampleRateHz;" in text:
        text = text.replace(
            "sampleRate = sourceTake.SampleRateHz;",
            "sampleRate = sourceTake.SampleRateHz ?? 0;",
            1,
        )
        text = text.replace(
            "totalFrames = (long)Math.Round(sourceTake.DurationMs * (double)sampleRate / 1000.0);",
            "totalFrames = (long)Math.Round((sourceTake.DurationMs ?? 0L) * (double)sampleRate / 1000.0);",
            1,
        )
        print("Healed SampleRateHz/DurationMs nullables")

    # If assembly handler missing, inject a minimal correct version before A/B play handler
    if "BuildPhraseAssemblyPreview_Click" not in text:
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
            var sampleRate = 0; var channels = 0; var bits = 16;
            var dataOffset = -1; var dataSize = 0;
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
        var sr = infos[0].sampleRate; var ch = infos[0].channels; var bits = infos[0].bits;
        if (bits != 16) return false;
        if (infos.Any(i => i.sampleRate != sr || i.channels != ch || i.bits != bits)) return false;
        var totalData = infos.Sum(i => i.dataSize);
        using var ms = new MemoryStream();
        void Le16(int v) { ms.WriteByte((byte)(v & 0xff)); ms.WriteByte((byte)((v >> 8) & 0xff)); }
        void Le32(int v)
        {
            ms.WriteByte((byte)(v & 0xff)); ms.WriteByte((byte)((v >> 8) & 0xff));
            ms.WriteByte((byte)((v >> 16) & 0xff)); ms.WriteByte((byte)((v >> 24) & 0xff));
        }
        var ascii = System.Text.Encoding.ASCII;
        ms.Write(ascii.GetBytes("RIFF")); Le32(36 + totalData); ms.Write(ascii.GetBytes("WAVE"));
        ms.Write(ascii.GetBytes("fmt ")); Le32(16); Le16(1); Le16(ch); Le32(sr);
        Le32(sr * ch * 2); Le16(ch * 2); Le16(16);
        ms.Write(ascii.GetBytes("data")); Le32(totalData);
        for (var i = 0; i < parts.Count; i++)
        {
            var bytes = File.ReadAllBytes(parts[i]);
            ms.Write(bytes, infos[i].dataOffset, infos[i].dataSize);
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
            var id8 = req.RequestId.Length >= 8 ? req.RequestId.Substring(0, 8) : req.RequestId;
            var head = Path.Combine(dir, $"head-{id8}.wav");
            var tail = Path.Combine(dir, $"tail-{id8}.wav");
            var preview = Path.Combine(dir, $"preview-{id8}.wav");

            long sampleRate = 48000;
            long totalFrames = 0;
            if (!string.IsNullOrWhiteSpace(_waveformFilePath) &&
                string.Equals(_waveformFilePath, sourceTake.SourcePath, StringComparison.OrdinalIgnoreCase) &&
                _waveformSampleRate > 0 && _waveformTotalFrames > 1)
            {
                sampleRate = _waveformSampleRate;
                totalFrames = (long)Math.Min(_waveformTotalFrames, (ulong)long.MaxValue);
            }
            else if (sourceTake.SampleRateHz is int srHz && srHz > 0 &&
                     sourceTake.DurationMs is long durMs && durMs > 0)
            {
                sampleRate = srHz;
                totalFrames = (long)Math.Round(durMs * (double)sampleRate / 1000.0);
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

            try { _previewPlayer.Pause(); _previewPlayer.Source = null; } catch { }
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
        anchor = "    private async void PlayPhraseOriginalRange_Click"
        if anchor not in text:
            anchor = "    private void PlayPhraseOriginalRange_Click"
        if anchor not in text:
            anchor = "    private void LinkLatestTakeAsPhraseReplacement_Click"
        if anchor not in text:
            raise SystemExit("Cannot find A/B handlers for assembly")
        text = text.replace(anchor, methods + "\n    " + anchor.lstrip(), 1)
        print("Windows phrase assembly preview handlers wired")
    else:
        print("Windows phrase assembly preview already present")

    path.write_text(text, encoding="utf-8")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        # Android: no-op if already applied by prior successful CI; full body was applied once.
        path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
        if path.exists() and "buildPhraseAssemblyPreview" in path.read_text(encoding="utf-8"):
            print("Android phrase assembly preview already present")
        elif path.exists():
            # Minimal: skip heavy android inject on heal-only revisions if base Redesign lacks it
            # Re-run requires the full helper; for now only heal Windows when android missing is OK
            # because android was green on prior runs with the full script.
            print("Android assembly preview not in working tree yet (CI applies on clean checkout from Redesign)")
            # Fall through: we need android inject. Load from a simplified path.
            ensure_android_full()
        else:
            print("skip android path missing")
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_phrase_assembly_preview_ui:", target, "OK")
    return 0


def ensure_android_full() -> None:
    """Full Android inject (used on clean Redesign checkout)."""
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
    text = path.read_text(encoding="utf-8")
    if "fun buildPhraseAssemblyPreview" in text:
        print("Android already has assembly preview")
        return
    # Defer to no-op with message - android was building; if missing, fail loudly
    # Import the previous full android body via requiring playPhraseOriginalRange anchor only for status.
    if "fun playPhraseOriginalRange" not in text and "fun linkLatestTakeAsPhraseReplacement" not in text:
        raise SystemExit("Cannot inject Android assembly: phrase helpers missing")
    # Lightweight: only add a stub that reports not implemented if somehow missing - better fail
    # Restore full android from known-good: use simplified status-only to keep Windows green while
    # we rely on prior full android in successful android job... Actually android job needs inject.
    raise SystemExit("Full Android assembly body required; restore complete script")


if __name__ == "__main__":
    raise SystemExit(main())
