#!/usr/bin/env python3
"""Wire VAD region sidecar + per-region Whisper transcription in MainActivity."""
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
    if "VadRegionStore.write" in text and "per-region Whisper" in text:
        print("VAD+Whisper region wiring already present")
        return

    original = text

    # Track last-take regions for same-session transcribe
    field_anchor = "    private val liveVadRegions = ArrayList<Pair<Long, Long>>()\n"
    field_new = (
        field_anchor
        + "    private var lastTakeVadRegions: List<Pair<Long, Long>> = emptyList()\n"
        + "    private var lastTakeVadWavName: String? = null\n"
    )
    if "lastTakeVadRegions" not in text:
        if field_anchor not in text:
            raise SystemExit("liveVadRegions field anchor not found")
        text = text.replace(field_anchor, field_new, 1)

    # After complete WAV validation, persist regions
    save_anchor = (
        "        if (stagedFile == null || !isCompletePcmWav(stagedFile)) {\n"
        "            diagnosticsCapture.record(\"recording_validation\", \"failure\", mapOf(\n"
        "                \"staging_name\" to (stagedFile?.name ?: \"missing\"),\n"
        "                \"staging_exists\" to (stagedFile?.exists() == true),\n"
        "                \"staging_bytes\" to (stagedFile?.length() ?: -1L)))\n"
        "            statusText.text = interruptionNotice +\n"
        "                \"UNVERIFIED TAKE — WAV validation failed. The staged file was retained in app-private storage and was not exported.\"\n"
        "            return\n"
        "        }\n"
        "\n"
        "        val takeDetails = recordingTakeDetails(stagedFile)\n"
    )
    save_new = (
        "        if (stagedFile == null || !isCompletePcmWav(stagedFile)) {\n"
        "            diagnosticsCapture.record(\"recording_validation\", \"failure\", mapOf(\n"
        "                \"staging_name\" to (stagedFile?.name ?: \"missing\"),\n"
        "                \"staging_exists\" to (stagedFile?.exists() == true),\n"
        "                \"staging_bytes\" to (stagedFile?.length() ?: -1L)))\n"
        "            statusText.text = interruptionNotice +\n"
        "                \"UNVERIFIED TAKE — WAV validation failed. The staged file was retained in app-private storage and was not exported.\"\n"
        "            return\n"
        "        }\n"
        "\n"
        "        lastTakeVadRegions = liveVadRegions.toList()\n"
        "        lastTakeVadWavName = stagedFile.name\n"
        "        try {\n"
        "            VadRegionStore.write(stagedFile, lastTakeVadRegions)\n"
        "            // Keep a copy under filesDir even if staging is deleted after export.\n"
        "            val durable = File(filesDir, stagedFile.name.removeSuffix(\".wav\") + \".vad.json\")\n"
        "            if (durable.absolutePath != VadRegionStore.sidecarForWav(stagedFile).absolutePath) {\n"
        "                VadRegionStore.write(File(filesDir, stagedFile.name), lastTakeVadRegions)\n"
        "            }\n"
        "        } catch (_: Exception) { /* non-fatal */ }\n"
        "\n"
        "        val takeDetails = recordingTakeDetails(stagedFile)\n"
    )
    if "VadRegionStore.write" not in text:
        if save_anchor not in text:
            raise SystemExit("recording validation success anchor not found")
        text = text.replace(save_anchor, save_new, 1)

    # Replace full-file-only transcribe body core with region-aware path
    old_tx = (
        "                mainHandler.post { if (!isFinishing && !isDestroyed) transcriptionStatus.text = \"Loading Whisper tiny and transcribing ${pcm.size / 16000 / 60} min of audio…\"}\n"
        "                val modelPath = ensureTranscriptionModel()\n"
        "                val payload = nativeTranscribe(modelPath, pcm, \"auto\", false)\n"
        "                val json = try {\n"
    )
    # May or may not have try from UI guard - handle both
    if old_tx not in text:
        old_tx = (
            "                mainHandler.post { if (!isFinishing && !isDestroyed) transcriptionStatus.text = \"Loading Whisper tiny and transcribing ${pcm.size / 16000 / 60} min of audio…\"}\n"
            "                val modelPath = ensureTranscriptionModel()\n"
            "                val payload = nativeTranscribe(modelPath, pcm, \"auto\", false)\n"
            "                val json = JSONObject(payload)\n"
        )

    new_tx = (
        "                val modelPath = ensureTranscriptionModel()\n"
        "                // Prefer live VAD speech regions when available (same-session or sidecar).\n"
        "                val vadRegions = buildList<VadRegionStore.Region> {\n"
        "                    val fromSidecar = VadRegionStore.read(file)\n"
        "                    if (fromSidecar.isNotEmpty()) addAll(fromSidecar)\n"
        "                    else if (lastTakeVadRegions.isNotEmpty()) {\n"
        "                        addAll(VadRegionStore.readFromPairs(lastTakeVadRegions))\n"
        "                    }\n"
        "                }\n"
        "                val json: JSONObject\n"
        "                if (vadRegions.isNotEmpty()) {\n"
        "                    // per-region Whisper: timed captions aligned to Silero endpoints\n"
        "                    mainHandler.post {\n"
        "                        if (!isFinishing && !isDestroyed)\n"
        "                            transcriptionStatus.text =\n"
        "                                \"Transcribing ${vadRegions.size} VAD speech regions with Whisper…\"\n"
        "                    }\n"
        "                    val mergedSegments = org.json.JSONArray()\n"
        "                    val textParts = ArrayList<String>()\n"
        "                    var detected = \"unknown\"\n"
        "                    for ((index, region) in vadRegions.withIndex()) {\n"
        "                        val startSample = ((region.startMs * 16L).coerceAtLeast(0L)).toInt().coerceAtMost(pcm.size)\n"
        "                        val endSample = ((region.endMs * 16L).coerceAtLeast(0L)).toInt().coerceAtMost(pcm.size)\n"
        "                        if (endSample <= startSample) continue\n"
        "                        val slice = pcm.copyOfRange(startSample, endSample)\n"
        "                        mainHandler.post {\n"
        "                            if (!isFinishing && !isDestroyed)\n"
        "                                transcriptionStatus.text =\n"
        "                                    \"Whisper region ${index + 1}/${vadRegions.size}…\"\n"
        "                        }\n"
        "                        val regionPayload = nativeTranscribe(modelPath, slice, \"auto\", false)\n"
        "                        val regionJson = JSONObject(regionPayload)\n"
        "                        if (regionJson.has(\"error\")) continue\n"
        "                        val piece = regionJson.optString(\"text\", \"\").trim()\n"
        "                        if (piece.isEmpty()) continue\n"
        "                        if (detected == \"unknown\") {\n"
        "                            detected = regionJson.optString(\"detectedLanguage\", \"unknown\")\n"
        "                        }\n"
        "                        textParts.add(piece)\n"
        "                        mergedSegments.put(\n"
        "                            JSONObject()\n"
        "                                .put(\"startMs\", region.startMs)\n"
        "                                .put(\"endMs\", region.endMs)\n"
        "                                .put(\"text\", piece),\n"
        "                        )\n"
        "                    }\n"
        "                    if (textParts.isEmpty()) {\n"
        "                        // Fall back to full-file if regions yielded no text.\n"
        "                        mainHandler.post {\n"
        "                            if (!isFinishing && !isDestroyed)\n"
        "                                transcriptionStatus.text = \"VAD regions empty — full-file Whisper…\"\n"
        "                        }\n"
        "                        val payload = nativeTranscribe(modelPath, pcm, \"auto\", false)\n"
        "                        json = JSONObject(payload)\n"
        "                    } else {\n"
        "                        json = JSONObject()\n"
        "                            .put(\"text\", textParts.joinToString(\" \"))\n"
        "                            .put(\"detectedLanguage\", detected)\n"
        "                            .put(\"segments\", mergedSegments)\n"
        "                    }\n"
        "                } else {\n"
        "                    mainHandler.post {\n"
        "                        if (!isFinishing && !isDestroyed)\n"
        "                            transcriptionStatus.text =\n"
        "                                \"Loading Whisper tiny and transcribing ${pcm.size / 16000 / 60} min of audio…\"\n"
        "                    }\n"
        "                    val payload = nativeTranscribe(modelPath, pcm, \"auto\", false)\n"
        "                    json = JSONObject(payload)\n"
        "                }\n"
        "                // parse / error handling continues with json\n"
        "                val _payloadForGuard = json.toString()\n"
        "                val payload = _payloadForGuard\n"
        "                if (false) { val json = JSONObject(payload) }\n"
        "                if (json.has(\"error\")) throw IOException(json.optString(\"error\", \"Transcription failed.\"))\n"
    )

    # Safer smaller replace: only replace the modelPath + single nativeTranscribe + json assign block
    # Reset and do simpler injection after pcm decode
    text = original  # undo partial if we already mutated fields - re-apply field+save only

    # Re-apply field + save only then do precise transcribe replace
    text = MAIN.read_text(encoding="utf-8") if False else text
    # Actually original was start of function - rewrite cleanly

    text = MAIN.read_text(encoding="utf-8")
    if "VadRegionStore.write" in text and "per-region Whisper" in text:
        print("already wired")
        return

    if "lastTakeVadRegions" not in text:
        if field_anchor not in text:
            raise SystemExit("liveVadRegions field anchor not found")
        text = text.replace(field_anchor, field_new, 1)

    if "VadRegionStore.write" not in text:
        if save_anchor not in text:
            raise SystemExit("recording validation success anchor not found")
        text = text.replace(save_anchor, save_new, 1)

    # Precise: after pcm empty check, before modelPath
    pcm_anchor = (
        "                if (pcm.isEmpty()) throw IOException(\"The selected WAV contains no audio samples.\")\n"
        "                mainHandler.post { if (!isFinishing && !isDestroyed) transcriptionStatus.text = \"Loading Whisper tiny and transcribing ${pcm.size / 16000 / 60} min of audio…\"}\n"
        "                val modelPath = ensureTranscriptionModel()\n"
        "                val payload = nativeTranscribe(modelPath, pcm, \"auto\", false)\n"
    )
    pcm_new = (
        "                if (pcm.isEmpty()) throw IOException(\"The selected WAV contains no audio samples.\")\n"
        "                val modelPath = ensureTranscriptionModel()\n"
        "                val vadRegions = buildList<VadRegionStore.Region> {\n"
        "                    val fromSidecar = VadRegionStore.read(file)\n"
        "                    if (fromSidecar.isNotEmpty()) addAll(fromSidecar)\n"
        "                    else if (lastTakeVadRegions.isNotEmpty()) addAll(VadRegionStore.readFromPairs(lastTakeVadRegions))\n"
        "                }\n"
        "                val payload: String\n"
        "                if (vadRegions.isNotEmpty()) {\n"
        "                    // per-region Whisper — captions timed to Silero VAD endpoints\n"
        "                    mainHandler.post { if (!isFinishing && !isDestroyed) transcriptionStatus.text = \"Transcribing ${vadRegions.size} VAD speech regions with Whisper…\" }\n"
        "                    val mergedSegments = org.json.JSONArray()\n"
        "                    val textParts = ArrayList<String>()\n"
        "                    var detected = \"unknown\"\n"
        "                    for ((index, region) in vadRegions.withIndex()) {\n"
        "                        val startSample = ((region.startMs * 16L).coerceAtLeast(0L)).toInt().coerceAtMost(pcm.size)\n"
        "                        val endSample = ((region.endMs * 16L).coerceAtLeast(0L)).toInt().coerceAtMost(pcm.size)\n"
        "                        if (endSample <= startSample) continue\n"
        "                        val slice = pcm.copyOfRange(startSample, endSample)\n"
        "                        mainHandler.post { if (!isFinishing && !isDestroyed) transcriptionStatus.text = \"Whisper region ${index + 1}/${vadRegions.size}…\" }\n"
        "                        val regionPayload = nativeTranscribe(modelPath, slice, \"auto\", false)\n"
        "                        val regionJson = JSONObject(regionPayload)\n"
        "                        if (regionJson.has(\"error\")) continue\n"
        "                        val piece = regionJson.optString(\"text\", \"\").trim()\n"
        "                        if (piece.isEmpty()) continue\n"
        "                        if (detected == \"unknown\") detected = regionJson.optString(\"detectedLanguage\", \"unknown\")\n"
        "                        textParts.add(piece)\n"
        "                        mergedSegments.put(JSONObject().put(\"startMs\", region.startMs).put(\"endMs\", region.endMs).put(\"text\", piece))\n"
        "                    }\n"
        "                    payload = if (textParts.isEmpty()) {\n"
        "                        mainHandler.post { if (!isFinishing && !isDestroyed) transcriptionStatus.text = \"VAD regions produced no text — full-file Whisper…\" }\n"
        "                        nativeTranscribe(modelPath, pcm, \"auto\", false)\n"
        "                    } else {\n"
        "                        JSONObject().put(\"text\", textParts.joinToString(\" \")).put(\"detectedLanguage\", detected).put(\"segments\", mergedSegments).toString()\n"
        "                    }\n"
        "                } else {\n"
        "                    mainHandler.post { if (!isFinishing && !isDestroyed) transcriptionStatus.text = \"Loading Whisper tiny and transcribing ${pcm.size / 16000 / 60} min of audio…\" }\n"
        "                    payload = nativeTranscribe(modelPath, pcm, \"auto\", false)\n"
        "                }\n"
    )
    if pcm_anchor not in text:
        raise SystemExit("transcribe pcm/modelPath anchor not found")
    text = text.replace(pcm_anchor, pcm_new, 1)

    if text == original:
        raise SystemExit("no changes applied")
    MAIN.write_text(text, encoding="utf-8")
    print("VAD+Whisper region wiring applied")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "android"
    if target in ("all", "android"):
        ensure_android()
    print("ci_apply_vad_whisper_regions:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
