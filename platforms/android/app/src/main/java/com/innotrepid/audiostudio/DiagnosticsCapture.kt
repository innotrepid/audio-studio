package com.innotrepid.audiostudio

import android.content.Context
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.TimeZone

/**
 * Local-only, bounded diagnostic capture. It records operation metadata and errors,
 * never audio samples or audio contents. Export is explicitly user initiated.
 */
internal class DiagnosticsCapture(context: Context) {
    private val directory = File(context.filesDir, "diagnostics")
    private val logFile = File(directory, "audio-studio-diagnostics.jsonl")
    private val backupFile = File(directory, "audio-studio-diagnostics.previous.jsonl")
    private val lock = Any()

    fun record(event: String, outcome: String, details: Map<String, Any?> = emptyMap(), error: Throwable? = null) {
        synchronized(lock) {
            try {
                if (!directory.exists() && !directory.mkdirs()) return
                if (logFile.length() >= MAX_LOG_BYTES) {
                    if (backupFile.exists()) backupFile.delete()
                    if (logFile.exists()) logFile.renameTo(backupFile)
                }
                val stamp = SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSS'Z'", Locale.ROOT).apply {
                    timeZone = TimeZone.getTimeZone("UTC")
                }.format(Date())
                val json = JSONObject()
                    .put("timestamp_utc", stamp)
                    .put("event", event)
                    .put("outcome", outcome)
                    .put("android_sdk", android.os.Build.VERSION.SDK_INT)
                details.forEach { (key, value) ->
                    // Avoid serializing arbitrary objects or content; only simple metadata is accepted.
                    when (value) {
                        null -> json.put(key, JSONObject.NULL)
                        is String, is Number, is Boolean -> json.put(key, value)
                        else -> json.put(key, value.toString().take(500))
                    }
                }
                if (error != null) {
                    json.put("exception_type", error.javaClass.name)
                    json.put("exception_message", error.message ?: "")
                }
                FileOutputStream(logFile, true).bufferedWriter(Charsets.UTF_8).use {
                    it.append(json.toString())
                    it.newLine()
                }
            } catch (_: Exception) {
                // Diagnostics must never crash or block the recording/export workflow.
            }
        }
    }

    fun reportBytes(): ByteArray = synchronized(lock) {
        buildString {
            appendLine("Audio Studio local diagnostics")
            appendLine("Contains operation metadata and errors only; no audio samples are recorded.")
            appendLine("Current Android SDK: ${android.os.Build.VERSION.SDK_INT}")
            if (backupFile.isFile) {
                appendLine()
                appendLine("--- Previous log segment ---")
                append(backupFile.readText(Charsets.UTF_8))
            }
            if (logFile.isFile) {
                appendLine()
                appendLine("--- Current log segment ---")
                append(logFile.readText(Charsets.UTF_8))
            } else {
                appendLine()
                appendLine("No diagnostic events captured yet.")
            }
        }.toByteArray(Charsets.UTF_8)
    }

    companion object {
        private const val MAX_LOG_BYTES = 1024L * 1024L
    }
}
