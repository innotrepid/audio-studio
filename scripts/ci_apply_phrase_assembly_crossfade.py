#!/usr/bin/env python3
"""Patch phrase assembly WAV concat to apply a short linear crossfade."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# ~15 ms at typical rates; computed from sampleRate at runtime
CROSSFADE_MS = 15

ANDROID_MARKER = "fun concatPcm16WavFiles"
WINDOWS_MARKER = "ConcatPcm16WavFiles"


def ensure_android() -> None:
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
    text = path.read_text(encoding="utf-8")
    if "crossfadeFrames" in text and ANDROID_MARKER in text:
        print("Android phrase assembly crossfade already present")
        return
    if ANDROID_MARKER not in text:
        raise SystemExit("concatPcm16WavFiles missing; run assembly preview apply first")

    # Replace the naive per-part write loop with crossfaded join
    old = '''            parts.forEachIndexed { index, file ->
                val bytes = file.readBytes()
                val info = infos[index]
                out.write(bytes, info.dataOffset, info.dataSize)
            }
            output.writeBytes(out.toByteArray())
            return output.isFile && output.length() > 44L
        }'''

    new = f'''            // Soft linear crossfade (~{CROSSFADE_MS} ms) between consecutive parts
            val frameBytes = ch * 2
            val crossfadeFrames = ((sr * {CROSSFADE_MS}) / 1000).coerceAtLeast(1)
            val pcmParts = parts.mapIndexed {{ index, file ->
                val bytes = file.readBytes()
                val info = infos[index]
                bytes.copyOfRange(info.dataOffset, info.dataOffset + info.dataSize)
            }}
            if (pcmParts.isEmpty()) return false
            val assembled = java.io.ByteArrayOutputStream()
            assembled.write(pcmParts[0])
            for (i in 1 until pcmParts.size) {{
                val prev = assembled.toByteArray()
                val next = pcmParts[i]
                val fadeBytes = (crossfadeFrames * frameBytes).coerceAtMost(prev.size).coerceAtMost(next.size)
                val fadeFrames = fadeBytes / frameBytes
                if (fadeFrames <= 0) {{
                    assembled.write(next)
                    continue
                }}
                val keepPrev = prev.size - fadeFrames * frameBytes
                val mixed = ByteArray(keepPrev + next.size)
                System.arraycopy(prev, 0, mixed, 0, keepPrev)
                var outPos = keepPrev
                for (f in 0 until fadeFrames) {{
                    val t = (f + 1).toFloat() / (fadeFrames + 1).toFloat()
                    for (c in 0 until ch) {{
                        val pi = keepPrev + f * frameBytes + c * 2
                        val ni = f * frameBytes + c * 2
                        val a = ((prev[pi].toInt() and 0xff) or (prev[pi + 1].toInt() shl 8)).toShort().toInt()
                        val b = ((next[ni].toInt() and 0xff) or (next[ni + 1].toInt() shl 8)).toShort().toInt()
                        val m = ((a * (1f - t)) + (b * t)).toInt().coerceIn(-32768, 32767)
                        mixed[outPos++] = (m and 0xff).toByte()
                        mixed[outPos++] = ((m shr 8) and 0xff).toByte()
                    }}
                }}
                val nextRest = fadeFrames * frameBytes
                System.arraycopy(next, nextRest, mixed, outPos, next.size - nextRest)
                assembled.reset()
                assembled.write(mixed)
            }}
            val pcm = assembled.toByteArray()
            val header = java.io.ByteArrayOutputStream()
            fun h16(v: Int) {{ header.write(v and 0xff); header.write((v shr 8) and 0xff) }}
            fun h32(v: Int) {{
                header.write(v and 0xff); header.write((v shr 8) and 0xff)
                header.write((v shr 16) and 0xff); header.write((v shr 24) and 0xff)
            }}
            header.write("RIFF".toByteArray()); h32(36 + pcm.size); header.write("WAVE".toByteArray())
            header.write("fmt ".toByteArray()); h32(16); h16(1); h16(ch); h32(sr)
            h32(sr * ch * 2); h16(ch * 2); h16(16)
            header.write("data".toByteArray()); h32(pcm.size)
            output.writeBytes(header.toByteArray() + pcm)
            return output.isFile && output.length() > 44L
        }}'''

    if old not in text:
        # Already patched or different formatting
        if "Soft linear crossfade" in text:
            print("Android crossfade already present")
            return
        raise SystemExit("Cannot find Android concat write loop to patch for crossfade")

    text = text.replace(old, new, 1)
    # Drop the earlier naive header+write path before the loop: the function still builds `out` header
    # Redirect: after computing totalData, skip writing header into `out` and only use crossfade path.
    # The existing function writes header to `out` then parts — we replaced the parts loop and final write
    # but left header on `out`. Fix by removing unused `out` header write when crossfade present is complex.
    # Simpler approach: leave header write on `out` unused; final write uses header+pcm. OK.
    path.write_text(text, encoding="utf-8")
    print("Android phrase assembly crossfade wired")


def ensure_windows() -> None:
    path = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml.cs"
    text = path.read_text(encoding="utf-8")
    if "crossfadeFrames" in text and WINDOWS_MARKER in text:
        print("Windows phrase assembly crossfade already present")
        return
    if WINDOWS_MARKER not in text:
        raise SystemExit("ConcatPcm16WavFiles missing; run assembly preview apply first")

    old = '''        for (var i = 0; i < parts.Count; i++)
        {
            var bytes = File.ReadAllBytes(parts[i]);
            var info = infos[i];
            ms.Write(bytes, info.dataOffset, info.dataSize);
        }
        File.WriteAllBytes(outputPath, ms.ToArray());
        return File.Exists(outputPath) && new FileInfo(outputPath).Length > 44;
    }'''

    # Also handle compacted form without info variable name variants
    if old not in text:
        old2 = '''        for (var i = 0; i < parts.Count; i++)
        {
            var bytes = File.ReadAllBytes(parts[i]);
            ms.Write(bytes, infos[i].dataOffset, infos[i].dataSize);
        }
        File.WriteAllBytes(outputPath, ms.ToArray());
        return File.Exists(outputPath) && new FileInfo(outputPath).Length > 44;
    }'''
        if old2 in text:
            old = old2
        elif "Soft linear crossfade" in text:
            print("Windows crossfade already present")
            return
        else:
            raise SystemExit("Cannot find Windows concat write loop to patch for crossfade")

    new = f'''        // Soft linear crossfade (~{CROSSFADE_MS} ms) between consecutive parts
        var frameBytes = ch * 2;
        var crossfadeFrames = Math.Max(1, (sr * {CROSSFADE_MS}) / 1000);
        var pcmParts = new List<byte[]>();
        for (var i = 0; i < parts.Count; i++)
        {{
            var bytes = File.ReadAllBytes(parts[i]);
            var info = infos[i];
            var pcm = new byte[info.dataSize];
            Buffer.BlockCopy(bytes, info.dataOffset, pcm, 0, info.dataSize);
            pcmParts.Add(pcm);
        }}
        var assembled = new List<byte>(pcmParts[0]);
        for (var i = 1; i < pcmParts.Count; i++)
        {{
            var prev = assembled.ToArray();
            var next = pcmParts[i];
            var fadeBytes = Math.Min(crossfadeFrames * frameBytes, Math.Min(prev.Length, next.Length));
            var fadeFrames = fadeBytes / frameBytes;
            if (fadeFrames <= 0)
            {{
                assembled.AddRange(next);
                continue;
            }}
            var keepPrev = prev.Length - fadeFrames * frameBytes;
            var mixed = new byte[keepPrev + next.Length];
            Buffer.BlockCopy(prev, 0, mixed, 0, keepPrev);
            var outPos = keepPrev;
            for (var f = 0; f < fadeFrames; f++)
            {{
                var t = (f + 1f) / (fadeFrames + 1f);
                for (var c = 0; c < ch; c++)
                {{
                    var pi = keepPrev + f * frameBytes + c * 2;
                    var ni = f * frameBytes + c * 2;
                    short a = (short)(prev[pi] | (prev[pi + 1] << 8));
                    short b = (short)(next[ni] | (next[ni + 1] << 8));
                    var m = (int)(a * (1f - t) + b * t);
                    if (m > 32767) m = 32767;
                    if (m < -32768) m = -32768;
                    mixed[outPos++] = (byte)(m & 0xff);
                    mixed[outPos++] = (byte)((m >> 8) & 0xff);
                }}
            }}
            var nextRest = fadeFrames * frameBytes;
            Buffer.BlockCopy(next, nextRest, mixed, outPos, next.Length - nextRest);
            assembled = new List<byte>(mixed);
        }}
        var pcmOut = assembled.ToArray();
        using var header = new MemoryStream();
        void H16(int v) {{ header.WriteByte((byte)(v & 0xff)); header.WriteByte((byte)((v >> 8) & 0xff)); }}
        void H32(int v)
        {{
            header.WriteByte((byte)(v & 0xff)); header.WriteByte((byte)((v >> 8) & 0xff));
            header.WriteByte((byte)((v >> 16) & 0xff)); header.WriteByte((byte)((v >> 24) & 0xff));
        }}
        var ascii = System.Text.Encoding.ASCII;
        header.Write(ascii.GetBytes("RIFF")); H32(36 + pcmOut.Length); header.Write(ascii.GetBytes("WAVE"));
        header.Write(ascii.GetBytes("fmt ")); H32(16); H16(1); H16(ch); H32(sr);
        H32(sr * ch * 2); H16(ch * 2); H16(16);
        header.Write(ascii.GetBytes("data")); H32(pcmOut.Length);
        var result = header.ToArray().Concat(pcmOut).ToArray();
        File.WriteAllBytes(outputPath, result);
        return File.Exists(outputPath) && new FileInfo(outputPath).Length > 44;
    }}'''

    text = text.replace(old, new, 1)
    path.write_text(text, encoding="utf-8")
    print("Windows phrase assembly crossfade wired")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_phrase_assembly_crossfade:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
