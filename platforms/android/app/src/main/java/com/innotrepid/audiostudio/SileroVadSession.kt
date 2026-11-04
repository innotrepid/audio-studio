package com.innotrepid.audiostudio

import android.content.Context

/**
 * Offline Silero VAD session backed by the packaged ONNX model and native engine.
 * Timestamps are milliseconds on the 16 kHz mono timeline after resampling.
 */
class SileroVadSession private constructor(
    private var handle: Long,
) : AutoCloseable {
    data class Region(val startMs: Long, val endMs: Long)

    data class PushResult(
        val ok: Boolean,
        val regions: List<Region>,
        val requiredCapacity: Int,
        val error: String?,
    )

    fun pushAudio(interleaved: FloatArray, frameCount: Int, capacity: Int = 32): PushResult {
        return deliver(capacity) { starts, ends, written ->
            nativePushAudio(handle, interleaved, frameCount, starts, ends, written)
        }
    }

    fun flush(capacity: Int = 32): PushResult {
        return deliver(capacity) { starts, ends, written ->
            nativeFlush(handle, starts, ends, written)
        }
    }

    fun reset(): Boolean = nativeReset(handle) == 1

    override fun close() {
        if (handle != 0L) {
            nativeDestroy(handle)
            handle = 0L
        }
    }

    private fun deliver(
        capacity: Int,
        block: (LongArray, LongArray, IntArray) -> Int,
    ): PushResult {
        val starts = LongArray(capacity.coerceAtLeast(0))
        val ends = LongArray(capacity.coerceAtLeast(0))
        val written = IntArray(1)
        val ok = block(starts, ends, written) == 1
        val count = written[0].coerceAtLeast(0)
        val regions = if (ok) {
            (0 until minOf(count, capacity)).map { Region(starts[it], ends[it]) }
        } else {
            emptyList()
        }
        return PushResult(
            ok = ok,
            regions = regions,
            requiredCapacity = count,
            error = if (ok) null else nativeLastError().ifBlank { "VAD operation failed." },
        )
    }

    companion object {
        init {
            System.loadLibrary("audio_studio_android")
        }

        fun open(
            context: Context,
            sampleRate: Int,
            channels: Int,
            speechThreshold: Float = 0.5f,
            minimumSpeechMs: Long = 100L,
            minimumSilenceMs: Long = 300L,
            paddingMs: Long = 80L,
        ): SileroVadSession {
            val models = BundledModelStore.resolve(context)
            val handle = nativeCreate(
                models.sileroVadPath,
                sampleRate,
                channels,
                speechThreshold,
                minimumSpeechMs,
                minimumSilenceMs,
                paddingMs,
            )
            if (handle == 0L) {
                throw IllegalStateException(
                    nativeLastError().ifBlank {
                        "Could not open Silero VAD with the bundled model."
                    },
                )
            }
            return SileroVadSession(handle)
        }

        @JvmStatic
        private external fun nativeCreate(
            modelPath: String,
            sampleRate: Int,
            channels: Int,
            speechThreshold: Float,
            minimumSpeechMs: Long,
            minimumSilenceMs: Long,
            paddingMs: Long,
        ): Long

        @JvmStatic
        private external fun nativeDestroy(handle: Long)

        @JvmStatic
        private external fun nativeLastError(): String

        @JvmStatic
        private external fun nativePushAudio(
            handle: Long,
            interleaved: FloatArray?,
            frameCount: Int,
            outStartsMs: LongArray,
            outEndsMs: LongArray,
            outWritten: IntArray,
        ): Int

        @JvmStatic
        private external fun nativeFlush(
            handle: Long,
            outStartsMs: LongArray,
            outEndsMs: LongArray,
            outWritten: IntArray,
        ): Int

        @JvmStatic
        private external fun nativeReset(handle: Long): Int
    }
}
