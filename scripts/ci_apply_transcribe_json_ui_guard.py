#!/usr/bin/env python3
"""Harden MainActivity transcription UI against malformed native JSON."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"


def ensure_android() -> None:
    if not MAIN.exists():
        print("MainActivity missing; skip")
        return
    text = MAIN.read_text(encoding="utf-8")
    if "Malformed transcription JSON" in text:
        print("Transcribe UI guard already present")
        return

    old = (
        "                val json = JSONObject(payload)\n"
        "                if (json.has(\"error\")) throw IOException(json.optString(\"error\", \"Transcription failed.\"))\n"
        "                val transcript = json.optString(\"text\", \"\").trim()\n"
        "                if (transcript.isEmpty()) throw IOException(\"Whisper returned an empty transcript.\")\n"
    )
    new = (
        "                val json = try {\n"
        "                    JSONObject(payload)\n"
        "                } catch (parseError: org.json.JSONException) {\n"
        "                    throw IOException(\n"
        "                        \"Malformed transcription JSON (native payload). \" +\n"
        "                            (parseError.message ?: \"parse failed\"),\n"
        "                        parseError,\n"
        "                    )\n"
        "                }\n"
        "                if (json.has(\"error\")) throw IOException(json.optString(\"error\", \"Transcription failed.\"))\n"
        "                val transcript = json.optString(\"text\", \"\").trim()\n"
        "                if (transcript.isEmpty()) {\n"
        "                    val brokenQuotes = payload.contains(\":)\")\n"
        "                    throw IOException(\n"
        "                        if (brokenQuotes)\n"
        "                            \"Transcription JSON missing text (engine returned a broken payload).\"\n"
        "                        else\n"
        "                            \"Whisper returned an empty transcript.\",\n"
        "                    )\n"
        "                }\n"
    )
    if old not in text:
        raise SystemExit("transcribe JSONObject anchor not found")
    MAIN.write_text(text.replace(old, new, 1), encoding="utf-8")
    print("Transcribe UI JSON guard applied")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "android"
    if target in ("all", "android"):
        ensure_android()
    print("ci_apply_transcribe_json_ui_guard:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
