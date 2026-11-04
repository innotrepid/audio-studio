package com.innotrepid.audiostudio

import android.content.Context

/**
 * Resolves the bundled Silero model path for live capture VAD.
 * Native dual-submit lives in native_bridge.cpp (LiveCaptureVad).
 * MainActivity applies [patches/mainactivity-live-vad.patch] to call
 * nativeStartRecordingWithVad / region poll APIs.
 */
object LiveVadRecording {
    data class StartPlan(
        val sileroModelPath: String?,
        val modelError: String?,
        val liveVadEnabled: Boolean,
    )

    fun plan(context: Context): StartPlan {
        val (models, error) = BundledModelStore.tryResolve(context)
        val path = models?.sileroVadPath
        return StartPlan(
            sileroModelPath = path,
            modelError = error,
            liveVadEnabled = path != null,
        )
    }
}
