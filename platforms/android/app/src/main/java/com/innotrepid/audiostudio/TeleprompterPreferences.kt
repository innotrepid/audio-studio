package com.innotrepid.audiostudio

import android.content.Context

/**
 * Persisted Creator → Teleprompter settings shared with the Record studio teleprompter.
 *
 * Keys are stable across app upgrades. Values use SeekBar/Spinner progress indices so UI
 * controls can restore without converting units twice.
 */
object TeleprompterPreferences {
    const val PREFS_NAME = "teleprompter_preferences"

    private const val KEY_SPEED_PROGRESS = "speed_progress"
    private const val KEY_FONT_PROGRESS = "font_progress"
    private const val KEY_SPACING_PROGRESS = "spacing_progress"
    private const val KEY_COUNTDOWN_INDEX = "countdown_index"

    /** Defaults match first-run SeekBar/Spinner positions in MainActivity. */
    data class State(
        val speedProgress: Int = 15,
        val fontProgress: Int = 5,
        val spacingProgress: Int = 3,
        val countdownIndex: Int = 0,
    )

    fun load(context: Context): State {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        return State(
            speedProgress = prefs.getInt(KEY_SPEED_PROGRESS, 15).coerceIn(0, 100),
            fontProgress = prefs.getInt(KEY_FONT_PROGRESS, 5).coerceIn(0, 20),
            spacingProgress = prefs.getInt(KEY_SPACING_PROGRESS, 3).coerceIn(0, 10),
            countdownIndex = prefs.getInt(KEY_COUNTDOWN_INDEX, 0).coerceIn(0, 3),
        )
    }

    fun save(
        context: Context,
        speedProgress: Int,
        fontProgress: Int,
        spacingProgress: Int,
        countdownIndex: Int,
    ) {
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .edit()
            .putInt(KEY_SPEED_PROGRESS, speedProgress.coerceIn(0, 100))
            .putInt(KEY_FONT_PROGRESS, fontProgress.coerceIn(0, 20))
            .putInt(KEY_SPACING_PROGRESS, spacingProgress.coerceIn(0, 10))
            .putInt(KEY_COUNTDOWN_INDEX, countdownIndex.coerceIn(0, 3))
            .apply()
    }

    fun speedPxPerSecond(progress: Int): Float = 24f + progress.coerceIn(0, 100) * 1.2f

    fun fontSizeSp(progress: Int): Int = 20 + progress.coerceIn(0, 20)

    fun lineSpacingMultiplier(progress: Int): Float = 1.2f + progress.coerceIn(0, 10) * 0.08f

    fun countdownSeconds(index: Int): Int = when (index.coerceIn(0, 3)) {
        1 -> 5
        2 -> 10
        3 -> 0
        else -> 3
    }
}
