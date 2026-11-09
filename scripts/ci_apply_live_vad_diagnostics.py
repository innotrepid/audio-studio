#!/usr/bin/env python3
"""Improve live Silero VAD status feedback on Android Record."""
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
    if "listening for speech endpoints" in text and "VAD regions this take" in text:
        print("Live VAD diagnostics already applied")
        return

    original = text

    # 1) Richer poll loop
    old_poll = (
        "    private val liveVadPollClock = object : Runnable {\n"
        "        override fun run() {\n"
        "            if (!recording) return\n"
        "            try {\n"
        "                while (nativePendingVadRegions() > 0) {\n"
        "                    if (!nativePopVadRegion(liveVadRegionPair)) break\n"
        "                    liveVadRegions.add(liveVadRegionPair[0] to liveVadRegionPair[1])\n"
        "                }\n"
        "                if (liveVadRegions.isNotEmpty()) {\n"
        "                    val last = liveVadRegions.last()\n"
        "                    val seconds = last.second / 1000.0\n"
        "                    statusText.text = \"Recording · VAD regions ${liveVadRegions.size} · last ends ${\"%.1f\".format(seconds)}s\"\n"
        "                }\n"
        "            } catch (_: Exception) {\n"
        "                // Soft-fail region UI only; recording continues.\n"
        "            }\n"
        "            mainHandler.postDelayed(this, 250L)\n"
        "        }\n"
        "    }\n"
    )
    # Note: Kotlin string in file uses ${} - need exact match from source
    # Re-fetch exact bytes with a smaller unique middle replace approach.

    old_inner = (
        "                if (liveVadRegions.isNotEmpty()) {\n"
        "                    val last = liveVadRegions.last()\n"
        "                    val seconds = last.second / 1000.0\n"
        "                    statusText.text = \"Recording · VAD regions ${liveVadRegions.size} · last ends ${\"%.1f\".format(seconds)}s\"\n"
        "                }\n"
        "            } catch (_: Exception) {\n"
        "                // Soft-fail region UI only; recording continues.\n"
        "            }\n"
    )
    new_inner = (
        "                if (liveVadRegions.isNotEmpty()) {\n"
        "                    val last = liveVadRegions.last()\n"
        "                    val seconds = last.second / 1000.0\n"
        "                    statusText.text = \"Recording · VAD regions ${liveVadRegions.size} · last ends ${\"%.1f\".format(seconds)}s\"\n"
        "                } else {\n"
        "                    statusText.text = \"Recording · live Silero VAD listening for speech endpoints\"\n"
        "                }\n"
        "            } catch (exception: Exception) {\n"
        "                statusText.text = \"Recording · VAD status unavailable: ${exception.message ?: \"error\"}\"\n"
        "                // Soft-fail region UI only; recording continues.\n"
        "            }\n"
    )
    if old_inner not in text:
        raise SystemExit("live VAD poll inner anchor not found")
    text = text.replace(old_inner, new_inner, 1)

    # 2) Drain pending regions once more at start of stopRecording, before removeCallbacks
    stop_anchor = (
        "        if (!recording) return\n"
        "        recording = false\n"
        "        mainHandler.removeCallbacks(liveVadPollClock)\n"
    )
    stop_new = (
        "        if (!recording) return\n"
        "        recording = false\n"
        "        try {\n"
        "            while (nativePendingVadRegions() > 0) {\n"
        "                if (!nativePopVadRegion(liveVadRegionPair)) break\n"
        "                liveVadRegions.add(liveVadRegionPair[0] to liveVadRegionPair[1])\n"
        "            }\n"
        "        } catch (_: Exception) {\n"
        "            // Keep stop path resilient.\n"
        "        }\n"
        "        mainHandler.removeCallbacks(liveVadPollClock)\n"
    )
    if "Drain pending VAD" not in text and "while (nativePendingVadRegions() > 0)" not in text[text.find("fun stopRecording"):text.find("fun stopRecording")+400]:
        if stop_anchor not in text:
            raise SystemExit("stopRecording VAD drain anchor not found")
        text = text.replace(stop_anchor, stop_new, 1)

    # 3) After successful finalize path — find a unique success status assignment
    # Look for common pattern after Saved recording
    # Prefer appending near interruptionNotice usage success branch.
    # Search for status after staged file saved - "Recording saved"
    markers = [
        'statusText.text = interruptionNotice + "Recording saved',
        'statusText.text = interruptionNotice + "Saved',
        '"Recording finalized',
    ]
    # Simpler: after diagnosticsCapture.record("recording_finalize" success path continues;
    # inject vad summary variable before nativeStatus check fails.
    # Append after nativeStopRecording block:
    native_block_end = (
        "        val nativeStatus = try {\n"
        "            nativeStopRecording()\n"
        "        } catch (exception: Exception) {\n"
        "            \"ERROR: Could not finalize recording: ${exception.message ?: \"unknown error\"}\"\n"
        "        }\n"
    )
    native_with_summary = (
        "        val nativeStatus = try {\n"
        "            nativeStopRecording()\n"
        "        } catch (exception: Exception) {\n"
        "            \"ERROR: Could not finalize recording: ${exception.message ?: \"unknown error\"}\"\n"
        "        }\n"
        "        val vadSummary = if (liveVadRegions.isEmpty()) {\n"
        "            \"VAD regions this take: 0 (no speech endpoints closed — talk then pause, or VAD may have soft-failed).\"\n"
        "        } else {\n"
        "            val speechMs = liveVadRegions.sumOf { (it.second - it.first).coerceAtLeast(0L) }\n"
        "            \"VAD regions this take: ${liveVadRegions.size} · ~${\"%.1f\".format(speechMs / 1000.0)}s speech detected.\"\n"
        "        }\n"
    )
    if "VAD regions this take" not in text:
        if native_block_end not in text:
            raise SystemExit("nativeStopRecording status anchor not found")
        text = text.replace(native_block_end, native_with_summary, 1)

    # 4) Attach vadSummary to success status lines that use interruptionNotice
    # Replace first success-ish status after finalize if we can find "Recording saved" or similar
    # Fallback: append to failure incomplete and a generic line is hard.
    # Find: statusText after successful save - search common patterns in file
    for old_status, tag in [
        (
            'statusText.text = interruptionNotice + "Recording saved',
            "saved",
        ),
    ]:
        pass

    # Patch any statusText that starts with interruptionNotice and mentions private storage / saved
    # Safer approach: inject into diagnostics map already present
    diag_old = (
        '        diagnosticsCapture.record("recording_finalize", if (nativeStatus.startsWith("Saved recording:")) "success" else "failure",\n'
        '            mapOf("native_status" to nativeStatus.replace(filesDir.absolutePath, "<app-private>").take(1200), "staging_name" to (stagedFile?.name ?: "missing"),\n'
        '                "staging_exists" to (stagedFile?.exists() == true), "staging_bytes" to (stagedFile?.length() ?: -1L)))\n'
    )
    diag_new = (
        '        diagnosticsCapture.record("recording_finalize", if (nativeStatus.startsWith("Saved recording:")) "success" else "failure",\n'
        '            mapOf("native_status" to nativeStatus.replace(filesDir.absolutePath, "<app-private>").take(1200), "staging_name" to (stagedFile?.name ?: "missing"),\n'
        '                "staging_exists" to (stagedFile?.exists() == true), "staging_bytes" to (stagedFile?.length() ?: -1L),\n'
        '                "vad_regions" to liveVadRegions.size, "vad_summary" to vadSummary.take(200)))\n'
    )
    if "vad_regions" not in text:
        if diag_old not in text:
            print("WARN: diagnostics finalize map anchor not found; skip diag enrich")
        else:
            text = text.replace(diag_old, diag_new, 1)

    # Show vadSummary on UI when finalize fails or succeeds — append to incomplete message
    fail_old = (
        "            statusText.text = interruptionNotice + \"RECOVERABLE / INCOMPLETE TAKE — finalization did not succeed.\\n\\n\" + nativeStatus +\n"
        "                \"\\n\\nThe recoverable partial, if present, was retained in app-private storage. Open “Check for interrupted recordings” to inspect it.\"\n"
    )
    fail_new = (
        "            statusText.text = interruptionNotice + \"RECOVERABLE / INCOMPLETE TAKE — finalization did not succeed.\\n\\n\" + nativeStatus +\n"
        "                \"\\n\\n\" + vadSummary +\n"
        "                \"\\n\\nThe recoverable partial, if present, was retained in app-private storage. Open “Check for interrupted recordings” to inspect it.\"\n"
    )
    if fail_old in text:
        text = text.replace(fail_old, fail_new, 1)

    # Success path: find status after successful save - often sets selectedWavPath
    # Grep-like: after "if (!nativeStatus.startsWith" return; next blocks
    # Append vadSummary to a success status if pattern exists
    success_snip = "Recording saved to app-private storage"
    if success_snip in text and "vadSummary" in text:
        # try to extend first occurrence of a long status assignment - skip if too fragile
        pass

    # Clearer start status
    start_old = (
        '            statusText.text = if (sileroPath != null) {\n'
        '                "Recording in progress · live Silero VAD enabled"\n'
        '            } else {\n'
        '                "Recording in progress · audio is being captured"\n'
        '            }\n'
    )
    start_new = (
        '            statusText.text = if (sileroPath != null) {\n'
        '                "Recording · live Silero VAD listening for speech endpoints"\n'
        '            } else {\n'
        '                "Recording in progress · VAD unavailable (" + (modelError ?: "no model") + ")"\n'
        '            }\n'
    )
    if start_old in text:
        text = text.replace(start_old, start_new, 1)

    if text == original:
        raise SystemExit("Live VAD diagnostics produced no changes")
    MAIN.write_text(text, encoding="utf-8")
    print("Live VAD diagnostics applied")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "android"
    if target in ("all", "android"):
        ensure_android()
    print("ci_apply_live_vad_diagnostics:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
