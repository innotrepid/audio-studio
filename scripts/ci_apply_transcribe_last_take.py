#!/usr/bin/env python3
"""Offer transcribe-last-take when latestVerifiedTakeUri is set."""
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
    if "Transcribe last take" in text and "chooseWavForTranscription" in text:
        # may already have partial - check for dialog
        if "Transcribe last take?" in text or "Use last recording" in text:
            print("Transcribe last take already wired")
            return

    original = text

    old_choose = (
        "    private fun chooseWavForTranscription() {\n"
        "        if (recording || processing || transcribing || wavPickerActive || startAfterPermission) {\n"
        "            transcriptionStatus.text = \"Finish the current operation before starting transcription.\"\n"
        "            return\n"
        "        }\n"
        "        val picker = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {\n"
        "            addCategory(Intent.CATEGORY_OPENABLE)\n"
        "            type = \"audio/*\"\n"
        "        }\n"
        "        wavPickerActive = true\n"
        "        transcribeWavButton.isEnabled = false\n"
        "        try { startActivityForResult(picker, REQUEST_TRANSCRIPTION_WAV) }\n"
        "        catch (exception: Exception) {\n"
        "            wavPickerActive = false\n"
        "            transcribeWavButton.isEnabled = true\n"
    )

    new_choose = (
        "    private fun chooseWavForTranscription() {\n"
        "        if (recording || processing || transcribing || wavPickerActive || startAfterPermission) {\n"
        "            transcriptionStatus.text = \"Finish the current operation before starting transcription.\"\n"
        "            return\n"
        "        }\n"
        "        val lastUri = latestVerifiedTakeUri\n"
        "        if (lastUri != null) {\n"
        "            val regionHint = if (lastTakeVadRegions.isNotEmpty()) {\n"
        "                \" (${lastTakeVadRegions.size} VAD regions)\"\n"
        "            } else {\n"
        "                \"\"\n"
        "            }\n"
        "            AlertDialog.Builder(this)\n"
        "                .setTitle(\"Transcribe last take?\")\n"
        "                .setMessage(\n"
        "                    \"Use the latest finalized recording$regionHint with Whisper\" +\n"
        "                        (if (lastTakeVadRegions.isNotEmpty()) \" (Silero-timed captions)\" else \"\") +\n"
        "                        \", or pick a different audio file.\"\n"
        "                )\n"
        "                .setPositiveButton(\"Last take\") { _, _ ->\n"
        "                    transcriptionStatus.text = \"Transcribing last take…\"\n"
        "                    transcribeSelectedWav(lastUri)\n"
        "                }\n"
        "                .setNeutralButton(\"Pick file\") { _, _ -> openTranscriptionPicker() }\n"
        "                .setNegativeButton(\"Cancel\", null)\n"
        "                .show()\n"
        "            return\n"
        "        }\n"
        "        openTranscriptionPicker()\n"
        "    }\n"
        "\n"
        "    private fun openTranscriptionPicker() {\n"
        "        val picker = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {\n"
        "            addCategory(Intent.CATEGORY_OPENABLE)\n"
        "            type = \"audio/*\"\n"
        "        }\n"
        "        wavPickerActive = true\n"
        "        transcribeWavButton.isEnabled = false\n"
        "        try { startActivityForResult(picker, REQUEST_TRANSCRIPTION_WAV) }\n"
        "        catch (exception: Exception) {\n"
        "            wavPickerActive = false\n"
        "            transcribeWavButton.isEnabled = true\n"
    )

    if old_choose not in text:
        raise SystemExit("chooseWavForTranscription anchor not found")
    text = text.replace(old_choose, new_choose, 1)

    # Hint on successful export status strings
    for old, new in [
        (
            'interruptionNotice + "FINALIZED AND VERIFIED — saved WAV in Music/Audio Studio/Recordings."',
            'interruptionNotice + "FINALIZED AND VERIFIED — saved WAV in Music/Audio Studio/Recordings." +\n'
            '                        (if (lastTakeVadRegions.isNotEmpty()) " · ${lastTakeVadRegions.size} VAD regions · tap Transcribe WAV for Silero-timed captions" else " · tap Transcribe WAV for last take")',
        ),
        (
            'interruptionNotice + "FINALIZED AND VERIFIED — recording exported successfully, but the private staging copy could not be removed."',
            'interruptionNotice + "FINALIZED AND VERIFIED — recording exported successfully, but the private staging copy could not be removed." +\n'
            '                        (if (lastTakeVadRegions.isNotEmpty()) " · ${lastTakeVadRegions.size} VAD regions · tap Transcribe WAV" else "")',
        ),
    ]:
        if old in text and "tap Transcribe WAV" not in text[text.find(old):text.find(old)+200]:
            text = text.replace(old, new, 1)

    if text == original:
        raise SystemExit("no changes")
    MAIN.write_text(text, encoding="utf-8")
    print("Transcribe last take applied")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "android"
    if target in ("all", "android"):
        ensure_android()
    print("ci_apply_transcribe_last_take:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
