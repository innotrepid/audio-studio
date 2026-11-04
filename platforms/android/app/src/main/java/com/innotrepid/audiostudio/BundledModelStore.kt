package com.innotrepid.audiostudio

import android.content.Context
import java.io.File
import java.io.IOException

/**
 * Extracts packaged Whisper and Silero model assets to app-private storage once,
 * then returns absolute filesystem paths for native APIs. No network access.
 */
object BundledModelStore {
    const val WHISPER_TINY_ASSET = "models/ggml-tiny.bin"
    const val SILERO_VAD_ASSET = "models/silero_vad.onnx"

    const val WHISPER_TINY_FILE = "ggml-tiny.bin"
    const val SILERO_VAD_FILE = "silero_vad.onnx"

    data class ResolvedModels(
        val whisperTinyPath: String,
        val sileroVadPath: String,
    )

    fun resolve(context: Context): ResolvedModels {
        val dir = File(context.filesDir, "bundled-models").apply { mkdirs() }
        val whisper = ensureAssetCopied(context, WHISPER_TINY_ASSET, File(dir, WHISPER_TINY_FILE))
        val silero = ensureAssetCopied(context, SILERO_VAD_ASSET, File(dir, SILERO_VAD_FILE))
        return ResolvedModels(
            whisperTinyPath = whisper.absolutePath,
            sileroVadPath = silero.absolutePath,
        )
    }

    fun tryResolve(context: Context): Pair<ResolvedModels?, String?> {
        return try {
            resolve(context) to null
        } catch (error: Exception) {
            null to (error.message ?: "Could not prepare bundled offline models.")
        }
    }

    private fun ensureAssetCopied(context: Context, assetPath: String, destination: File): File {
        if (destination.isFile && destination.length() > 0L) {
            return destination
        }
        val parent = destination.parentFile
            ?: throw IOException("Invalid model destination: ${destination.path}")
        if (!parent.exists() && !parent.mkdirs()) {
            throw IOException("Could not create model directory: ${parent.path}")
        }
        val temporary = File(parent, destination.name + ".partial")
        try {
            context.assets.open(assetPath).use { input ->
                temporary.outputStream().use { output -> input.copyTo(output) }
            }
            if (temporary.length() <= 0L) {
                throw IOException("Bundled asset was empty: $assetPath")
            }
            if (destination.exists() && !destination.delete()) {
                throw IOException("Could not replace existing model file: ${destination.path}")
            }
            if (!temporary.renameTo(destination)) {
                temporary.copyTo(destination, overwrite = true)
                temporary.delete()
            }
        } catch (error: Exception) {
            temporary.delete()
            throw IOException(
                "Missing or unreadable bundled model asset '$assetPath'. " +
                    "Install a build that packages offline Whisper/Silero models.",
                error,
            )
        }
        return destination
    }
}
