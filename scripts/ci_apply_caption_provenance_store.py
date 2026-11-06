#!/usr/bin/env python3
"""Add timingProvenance to Creator project stores (Android + Windows)."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/CreatorProjectStore.kt"
    text = path.read_text(encoding="utf-8")
    if "timingProvenance" in text and "PROVENANCE_ESTIMATED" in text:
        print("Android store already has timing provenance")
        return
    old = """    data class Caption(
        val captionId: String,
        val startMs: Long,
        val endMs: Long,
        val text: String
    )
"""
    new = """    data class Caption(
        val captionId: String,
        val startMs: Long,
        val endMs: Long,
        val text: String,
        /** estimated | manual | unknown — never claims audio alignment unless "aligned" is added later. */
        val timingProvenance: String = PROVENANCE_UNKNOWN,
    ) {
        companion object {
            const val PROVENANCE_ESTIMATED = "estimated"
            const val PROVENANCE_MANUAL = "manual"
            const val PROVENANCE_UNKNOWN = "unknown"
        }
    }
"""
    if old not in text:
        raise SystemExit("Android Caption data class not found")
    text = text.replace(old, new, 1)
    text = text.replace(
        """            val caption = Caption(UUID.randomUUID().toString(), cursor, cursor + duration, chunk.joinToString(" "))
""",
        """            val caption = Caption(
                UUID.randomUUID().toString(),
                cursor,
                cursor + duration,
                chunk.joinToString(" "),
                Caption.PROVENANCE_ESTIMATED,
            )
""",
        1,
    )
    text = text.replace(
        """            require(caption.text.isNotBlank() && caption.text.length <= 1_000) { "Caption text must contain 1–1,000 characters." }
""",
        """            require(caption.text.isNotBlank() && caption.text.length <= 1_000) { "Caption text must contain 1–1,000 characters." }
            require(caption.timingProvenance in setOf(
                Caption.PROVENANCE_ESTIMATED,
                Caption.PROVENANCE_MANUAL,
                Caption.PROVENANCE_UNKNOWN,
            )) { "Caption timing provenance must be estimated, manual, or unknown." }
""",
        1,
    )
    text = text.replace(
        """                        .put("endMs", caption.endMs).put("text", caption.text))
""",
        """                        .put("endMs", caption.endMs).put("text", caption.text)
                        .put("timingProvenance", caption.timingProvenance))
""",
        1,
    )
    text = text.replace(
        """            Caption(item.getString("captionId"), item.getLong("startMs"), item.getLong("endMs"), item.getString("text"))
""",
        """            Caption(
                item.getString("captionId"),
                item.getLong("startMs"),
                item.getLong("endMs"),
                item.getString("text"),
                item.optString("timingProvenance", Caption.PROVENANCE_UNKNOWN).ifBlank { Caption.PROVENANCE_UNKNOWN },
            )
""",
        1,
    )
    path.write_text(text, encoding="utf-8")
    print("Android CreatorProjectStore timing provenance applied")


def ensure_windows() -> None:
    path = ROOT / "platforms/windows/AudioStudio/CreatorProjectStore.cs"
    text = path.read_text(encoding="utf-8")
    if "TimingProvenance" in text and "ProvenanceEstimated" in text:
        print("Windows store already has timing provenance")
        return
    old = """    internal sealed class CaptionDocument
    {
        public string CaptionId { get; set; } = Guid.NewGuid().ToString();
        public long StartMs { get; set; }
        public long EndMs { get; set; }
        public string Text { get; set; } = string.Empty;
    }
"""
    new = """    internal sealed class CaptionDocument
    {
        public const string ProvenanceEstimated = "estimated";
        public const string ProvenanceManual = "manual";
        public const string ProvenanceUnknown = "unknown";

        public string CaptionId { get; set; } = Guid.NewGuid().ToString();
        public long StartMs { get; set; }
        public long EndMs { get; set; }
        public string Text { get; set; } = string.Empty;
        /// <summary>estimated | manual | unknown — does not claim audio alignment.</summary>
        public string TimingProvenance { get; set; } = ProvenanceUnknown;
    }
"""
    if old not in text:
        raise SystemExit("Windows CaptionDocument not found")
    text = text.replace(old, new, 1)
    text = text.replace(
        """            var caption = new CaptionDocument { StartMs = cursor, EndMs = cursor + duration, Text = string.Join(" ", chunk) };
""",
        """            var caption = new CaptionDocument
            {
                StartMs = cursor,
                EndMs = cursor + duration,
                Text = string.Join(" ", chunk),
                TimingProvenance = CaptionDocument.ProvenanceEstimated,
            };
""",
        1,
    )
    text = text.replace(
        """            if (string.IsNullOrWhiteSpace(caption.Text) || caption.Text.Length > 1_000)
                throw new InvalidDataException("Caption text must contain 1–1,000 characters.");
""",
        """            if (string.IsNullOrWhiteSpace(caption.Text) || caption.Text.Length > 1_000)
                throw new InvalidDataException("Caption text must contain 1–1,000 characters.");
            var provenance = string.IsNullOrWhiteSpace(caption.TimingProvenance)
                ? CaptionDocument.ProvenanceUnknown
                : caption.TimingProvenance.Trim();
            if (provenance is not (CaptionDocument.ProvenanceEstimated or CaptionDocument.ProvenanceManual or CaptionDocument.ProvenanceUnknown))
                throw new InvalidDataException("Caption timing provenance must be estimated, manual, or unknown.");
            caption.TimingProvenance = provenance;
""",
        1,
    )
    path.write_text(text, encoding="utf-8")
    print("Windows CreatorProjectStore timing provenance applied")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_caption_provenance_store:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
