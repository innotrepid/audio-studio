#!/usr/bin/env python3
"""Add optional transcriptTakeId to Creator project stores."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/CreatorProjectStore.kt"
    text = path.read_text(encoding="utf-8")
    if "transcriptTakeId" in text:
        print("Android store already has transcriptTakeId")
        return

    text = text.replace(
        """        val transcript: String = "",
        val captions: List<Caption> = emptyList(),
""",
        """        val transcript: String = "",
        /** Optional take this transcript was prepared against; not a timed alignment claim. */
        val transcriptTakeId: String? = null,
        val captions: List<Caption> = emptyList(),
""",
        1,
    )

    # encode
    text = text.replace(
        '            .put("transcript", project.transcript)\n',
        '            .put("transcript", project.transcript)\n'
        '            .put("transcriptTakeId", project.transcriptTakeId ?: JSONObject.NULL)\n',
        1,
    )

    # decode
    text = text.replace(
        '            transcript = root.optString("transcript", ""),\n',
        '            transcript = root.optString("transcript", ""),\n'
        '            transcriptTakeId = root.optString("transcriptTakeId").takeIf { it.isNotBlank() && it != "null" },\n',
        1,
    )

    # validate optional take id if present
    if "transcriptTakeId" in text and "A transcriptTakeId must reference" not in text:
        text = text.replace(
            '        require(project.transcript.length <= maxSectionTextLength) { "Transcript exceeds the 100,000 character limit." }\n',
            '        require(project.transcript.length <= maxSectionTextLength) { "Transcript exceeds the 100,000 character limit." }\n'
            '        project.transcriptTakeId?.let { tid ->\n'
            '            require(tid.isNotBlank()) { "transcriptTakeId must not be blank when set." }\n'
            '            require(project.takes.any { it.optString("takeId") == tid }) {\n'
            '                "A transcriptTakeId must reference an existing take in this project."\n'
            '            }\n'
            '        }\n',
            1,
        )

    path.write_text(text, encoding="utf-8")
    print("Android transcriptTakeId applied")


def ensure_windows() -> None:
    path = ROOT / "platforms/windows/AudioStudio/CreatorProjectStore.cs"
    text = path.read_text(encoding="utf-8")
    if "TranscriptTakeId" in text:
        print("Windows store already has TranscriptTakeId")
        return

    # ProjectDocument properties
    text = text.replace(
        "        public string Transcript { get; set; } = string.Empty;\n",
        "        public string Transcript { get; set; } = string.Empty;\n"
        "        /// <summary>Optional take this transcript was prepared against.</summary>\n"
        "        public string? TranscriptTakeId { get; set; }\n",
        1,
    )

    # Validate if present
    if "TranscriptTakeId must reference" not in text:
        # after transcript length checks if any, or near captions validate
        anchor = "        foreach (var caption in project.Captions ?? new List<CaptionDocument>())"
        if anchor not in text:
            # try alternate
            pass
        else:
            insert = (
                "        if (!string.IsNullOrWhiteSpace(project.TranscriptTakeId))\n"
                "        {\n"
                "            var tid = project.TranscriptTakeId.Trim();\n"
                "            if (project.Takes is null || !project.Takes.Any(t => t.TakeId == tid))\n"
                "                throw new InvalidDataException(\"A transcriptTakeId must reference an existing take in this project.\");\n"
                "            project.TranscriptTakeId = tid;\n"
                "        }\n"
                "        else project.TranscriptTakeId = null;\n\n"
            )
            text = text.replace(anchor, insert + anchor, 1)

    path.write_text(text, encoding="utf-8")
    print("Windows TranscriptTakeId applied")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_transcript_take_link_store:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
