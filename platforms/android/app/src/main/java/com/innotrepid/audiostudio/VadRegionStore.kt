package com.innotrepid.audiostudio

import org.json.JSONArray
import org.json.JSONObject
import java.io.File

/**
 * Sidecar speech regions from live Silero VAD for offline Whisper caption timing.
 * File format: {"regions":[{"startMs":0,"endMs":1200}, ...]}
 */
object VadRegionStore {
    data class Region(val startMs: Long, val endMs: Long)

    fun sidecarForWav(wav: File): File {
        val base = wav.name.removeSuffix(".wav").removeSuffix(".WAV")
        return File(wav.parentFile ?: wav.absoluteFile.parentFile, "$base.vad.json")
    }

    fun write(wav: File, regions: List<Pair<Long, Long>>): File? {
        if (regions.isEmpty()) return null
        val out = sidecarForWav(wav)
        val arr = JSONArray()
        for ((start, end) in regions) {
            if (end <= start) continue
            arr.put(
                JSONObject()
                    .put("startMs", start)
                    .put("endMs", end),
            )
        }
        if (arr.length() == 0) return null
        out.writeText(JSONObject().put("regions", arr).toString())
        return out
    }

    fun read(wav: File): List<Region> {
        val side = sidecarForWav(wav)
        if (!side.isFile) return emptyList()
        return try {
            val root = JSONObject(side.readText())
            val arr = root.optJSONArray("regions") ?: return emptyList()
            buildList {
                for (i in 0 until arr.length()) {
                    val o = arr.optJSONObject(i) ?: continue
                    val start = o.optLong("startMs", -1L)
                    val end = o.optLong("endMs", -1L)
                    if (start >= 0L && end > start) add(Region(start, end))
                }
            }
        } catch (_: Exception) {
            emptyList()
        }
    }

    fun readFromPairs(pairs: List<Pair<Long, Long>>): List<Region> =
        pairs.mapNotNull { (s, e) -> if (e > s) Region(s, e) else null }
}
