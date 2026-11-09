#!/usr/bin/env python3
"""Upgrade MainActivity VAD region resolve/save to use durable filesDir index."""
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
    if "VadRegionStore.writeDurable" in text and "readDurable" in text:
        print("Durable VAD sidecar already wired")
        return

    if "VadRegionStore.write" not in text:
        print("Base VAD+Whisper wiring missing; run ci_apply_vad_whisper_regions first")
        return

    original = text

    # Strengthen save block to always writeDurable
    old_save = (
        "        try {\n"
        "            VadRegionStore.write(stagedFile, lastTakeVadRegions)\n"
        "            // Keep a copy under filesDir even if staging is deleted after export.\n"
        "            val durable = File(filesDir, stagedFile.name.removeSuffix(\".wav\") + \".vad.json\")\n"
        "            if (durable.absolutePath != VadRegionStore.sidecarForWav(stagedFile).absolutePath) {\n"
        "                VadRegionStore.write(File(filesDir, stagedFile.name), lastTakeVadRegions)\n"
        "            }\n"
        "        } catch (_: Exception) { /* non-fatal */ }\n"
    )
    new_save = (
        "        try {\n"
        "            VadRegionStore.write(stagedFile, lastTakeVadRegions)\n"
        "            VadRegionStore.writeDurable(this@MainActivity, stagedFile.name, lastTakeVadRegions)\n"
        "        } catch (_: Exception) { /* non-fatal */ }\n"
    )
    if old_save in text:
        text = text.replace(old_save, new_save, 1)
    elif "VadRegionStore.writeDurable" not in text and "VadRegionStore.write(stagedFile" in text:
        text = text.replace(
            "            VadRegionStore.write(stagedFile, lastTakeVadRegions)\n",
            "            VadRegionStore.write(stagedFile, lastTakeVadRegions)\n"
            "            VadRegionStore.writeDurable(this@MainActivity, stagedFile.name, lastTakeVadRegions)\n",
            1,
        )

    # Expand region resolution
    old_resolve = (
        "                val vadRegions = buildList<VadRegionStore.Region> {\n"
        "                    val fromSidecar = VadRegionStore.read(file)\n"
        "                    if (fromSidecar.isNotEmpty()) addAll(fromSidecar)\n"
        "                    else if (lastTakeVadRegions.isNotEmpty()) addAll(VadRegionStore.readFromPairs(lastTakeVadRegions))\n"
        "                }\n"
    )
    new_resolve = (
        "                val vadRegions = buildList<VadRegionStore.Region> {\n"
        "                    val fromSidecar = VadRegionStore.read(file)\n"
        "                    if (fromSidecar.isNotEmpty()) addAll(fromSidecar)\n"
        "                    val fromDurable = VadRegionStore.readDurable(this@MainActivity, file.name)\n"
        "                    if (isEmpty() && fromDurable.isNotEmpty()) addAll(fromDurable)\n"
        "                    val fromLastName = lastTakeVadWavName?.let { VadRegionStore.readDurable(this@MainActivity, it) }.orEmpty()\n"
        "                    if (isEmpty() && fromLastName.isNotEmpty()) addAll(fromLastName)\n"
        "                    if (isEmpty() && lastTakeVadRegions.isNotEmpty()) addAll(VadRegionStore.readFromPairs(lastTakeVadRegions))\n"
        "                }\n"
    )
    if old_resolve in text:
        text = text.replace(old_resolve, new_resolve, 1)
    else:
        print("WARN: vadRegions resolve anchor not found (base apply shape may differ)")

    if text == original:
        print("No durable VAD changes applied")
        return
    MAIN.write_text(text, encoding="utf-8")
    print("Durable VAD sidecar wiring applied")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "android"
    if target in ("all", "android"):
        ensure_android()
    print("ci_apply_vad_sidecar_durable:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
