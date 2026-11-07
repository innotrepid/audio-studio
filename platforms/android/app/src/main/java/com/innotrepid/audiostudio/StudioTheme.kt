package com.innotrepid.audiostudio

import android.app.Activity
import android.content.Context
import android.content.res.Configuration
import android.graphics.Color
import android.os.Build
import android.view.View
import android.view.WindowManager

/**
 * Hybrid Studio Phase E — theme preference application.
 *
 * Preference key shared with [SettingsScreen]. Resolves System default vs forced
 * Dark studio and applies window chrome. Panel colors in MainActivity remain the
 * dark studio palette; System-light only lightens the outer window shell so we
 * do not rewrite every control color in one risky pass.
 */
object StudioTheme {
    const val PREFS = "synvidra_app_preferences"
    const val KEY_THEME = "theme_mode" // 0 = system, 1 = dark studio

    const val MODE_SYSTEM = 0
    const val MODE_DARK_STUDIO = 1

    data class Palette(
        val windowBackground: Int,
        val surface: Int,
        val statusBar: Int,
        val navigationBar: Int,
        val lightStatusIcons: Boolean,
    )

    /** Charcoal studio tokens (docs/THEME_APP_WIDE.md). */
    val darkStudio = Palette(
        windowBackground = Color.rgb(16, 18, 24),
        surface = Color.rgb(28, 34, 45),
        statusBar = Color.rgb(12, 14, 20),
        navigationBar = Color.rgb(12, 14, 20),
        lightStatusIcons = false,
    )

    /** Softer shell when System is in light mode — content panels stay studio-dark. */
    val systemLightShell = Palette(
        windowBackground = Color.rgb(236, 239, 244),
        surface = Color.rgb(28, 34, 45),
        statusBar = Color.rgb(236, 239, 244),
        navigationBar = Color.rgb(236, 239, 244),
        lightStatusIcons = true,
    )

    fun storedMode(context: Context): Int {
        return context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
            .getInt(KEY_THEME, MODE_SYSTEM)
            .coerceIn(MODE_SYSTEM, MODE_DARK_STUDIO)
    }

    fun setMode(context: Context, mode: Int) {
        context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
            .edit()
            .putInt(KEY_THEME, mode.coerceIn(MODE_SYSTEM, MODE_DARK_STUDIO))
            .apply()
    }

    fun isNightUi(context: Context): Boolean {
        val mask = context.resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK
        return mask == Configuration.UI_MODE_NIGHT_YES
    }

    fun resolvedPalette(context: Context): Palette {
        return when (storedMode(context)) {
            MODE_DARK_STUDIO -> darkStudio
            else -> if (isNightUi(context)) darkStudio else systemLightShell
        }
    }

    /** Apply status bar, navigation bar, and decor background. Safe on plain Activity. */
    fun applyWindowChrome(activity: Activity) {
        val palette = resolvedPalette(activity)
        val window = activity.window
        window.addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS)
        window.statusBarColor = palette.statusBar
        window.navigationBarColor = palette.navigationBar
        window.decorView.setBackgroundColor(palette.windowBackground)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            var flags = window.decorView.systemUiVisibility
            flags = if (palette.lightStatusIcons) {
                flags or View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR
            } else {
                flags and View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR.inv()
            }
            window.decorView.systemUiVisibility = flags
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            var flags = window.decorView.systemUiVisibility
            flags = if (palette.lightStatusIcons) {
                flags or View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR
            } else {
                flags and View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR.inv()
            }
            window.decorView.systemUiVisibility = flags
        }
    }
}
