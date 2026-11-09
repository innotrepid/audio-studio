package com.innotrepid.audiostudio

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

/**
 * Sidecar speech regions from live Silero VAD for offline Whisper caption timing.
 * File format: {"regions":[{"startMs":0,"endMs":1200}, ...]}
 *
 * Durable copies live under filesDir/vad-regions/ so regions survive staging
 * deletion after MediaStore export.
 */
object VadRegionStore {
    data class Region(val startMs: Long, val endMs: Long)

    fun sidecarForWav(wav: File): File {
        val base = wav.name.removeSuffix(".wav").removeSuffix(".WAV")
        return File(wav.parentFile ?: wav.absoluteFile.parentFile, "$base.vad.json")
    }

    fun durableDir(context: Context): File {
        return File(context.filesDir, "vad-regions").also { it.mkdirs() }
    }

    fun durableForName(context: Context, wavName: String): File {
        val base = wavName.removeSuffix(".wav").removeSuffix(".WAV")
        return File(durableDir(context), "$base.vad.json")
    }

    private fun writeJson(out: File, regions: List<Pair<Long, Long>>): Boolean {
        val arr = JSONArray()
        for ((start, end) in regions) {
            if (end <= start) continue
            arr.put(
                JSONObject()
                    .put("startMs", start)
                    .put("endMs", end),
            )
        }
        if (arr.length() == 0) return false
        out.writeText(JSONObject().put("regions", arr).toString())
        return true
    }

    fun write(wav: File, regions: List<Pair<Long, Long>>): File? {
        if (regions.isEmpty()) return null
        val out = sidecarForWav(wav)
        return if (writeJson(out, regions)) out else null
    }

    /** App-private index keyed by WAV display name (survives export). */
    fun writeDurable(context: Context, wavName: String, regions: List<Pair<Long, Long>>): File? {
        if (regions.isEmpty() || wavName.isBlank()) return null
        val out = durableForName(context, wavName)
        return if (writeJson(out, regions)) out else null
    }

    private fun readFile(side: File): List<Region> {
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

    fun read(wav: File): List<Region> = readFile(sidecarForWav(wav))

    fun readDurable(context: Context, wavName: String): List<Region> {
        if (wavName.isBlank()) return emptyList()
        return readFile(durableForName(context, wavName))
    }

    fun readFromPairs(pairs: List<Pair<Long, Long>>): List<Region> =
        pairs.mapNotNull { (s, e) -> if (e > s) Region(s, e) else null }
}
