package com.innotrepid.audiostudio

import android.content.Context
import android.graphics.Color
import android.graphics.Typeface
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.RadioButton
import android.widget.RadioGroup
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast

/**
 * Additive Settings destination content for Hybrid Studio.
 * Built outside MainActivity so CI can wire navigation without mid-builder injects.
 */
object SettingsScreen {
    fun build(
        context: Context,
        density: Float,
        onResetProcessingToNatural: () -> Unit,
        onThemeChanged: (mode: Int) -> Unit = {},
    ): LinearLayout {
        fun dp(v: Int): Int = (v * density + 0.5f).toInt()

        val root = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(18), dp(18), dp(18), dp(18))
            background = android.graphics.drawable.GradientDrawable().apply {
                setColor(StudioTheme.darkStudio.surface)
                cornerRadius = 22f
            }
        }

        fun heading(text: String) = TextView(context).apply {
            this.text = text
            textSize = 20f
            setTextColor(Color.WHITE)
            setTypeface(null, Typeface.BOLD)
        }

        fun sectionLabel(text: String) = TextView(context).apply {
            this.text = text
            textSize = 12f
            setTypeface(null, Typeface.BOLD)
            setTextColor(Color.rgb(174, 185, 204))
            setPadding(0, dp(14), 0, dp(6))
        }

        fun body(text: String) = TextView(context).apply {
            this.text = text
            textSize = 13f
            setTextColor(Color.rgb(210, 218, 230))
        }

        val matchWidth = LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT,
        )

        root.addView(heading("Settings"), matchWidth)
        root.addView(
            body("App-wide preferences. Record, Editor, Creator, and Library stay available from the bottom navigation."),
            matchWidth,
        )

        root.addView(sectionLabel("RECORDING"), matchWidth)
        root.addView(
            body("Input device and sample rate remain on the Record workspace. Capture behavior is unchanged from Settings."),
            matchWidth,
        )

        root.addView(sectionLabel("PROCESSING"), matchWidth)
        root.addView(
            body("Presets and advanced controls live on Editor. Reset below restores the Natural preset values."),
            matchWidth,
        )
        root.addView(
            Button(context).apply {
                text = "Reset processing to Natural"
                contentDescription = "Reset processing preferences to Natural defaults"
                minHeight = dp(44)
                setOnClickListener {
                    onResetProcessingToNatural()
                    Toast.makeText(context, "Processing reset to Natural", Toast.LENGTH_SHORT).show()
                }
            },
            matchWidth,
        )

        root.addView(sectionLabel("APPEARANCE"), matchWidth)
        root.addView(
            body("System follows the device light/dark setting for the outer shell. Dark studio always uses charcoal chrome."),
            matchWidth,
        )
        val themeMode = StudioTheme.storedMode(context)
        val themeGroup = RadioGroup(context).apply { orientation = RadioGroup.VERTICAL }
        val systemRadio = RadioButton(context).apply {
            text = "System default"
            contentDescription = "Theme system default"
            id = View.generateViewId()
        }
        val darkRadio = RadioButton(context).apply {
            text = "Dark studio"
            contentDescription = "Theme dark studio"
            id = View.generateViewId()
        }
        themeGroup.addView(systemRadio)
        themeGroup.addView(darkRadio)
        themeGroup.check(if (themeMode == StudioTheme.MODE_DARK_STUDIO) darkRadio.id else systemRadio.id)
        themeGroup.setOnCheckedChangeListener { _, checkedId ->
            val mode = if (checkedId == darkRadio.id) {
                StudioTheme.MODE_DARK_STUDIO
            } else {
                StudioTheme.MODE_SYSTEM
            }
            StudioTheme.setMode(context, mode)
            onThemeChanged(mode)
        }
        root.addView(themeGroup, matchWidth)

        root.addView(sectionLabel("MODELS & OFFLINE"), matchWidth)
        root.addView(
            body("Bundled offline models: Whisper ggml-tiny and Silero VAD. License text: NOTICE.md in the repository."),
            matchWidth,
        )

        root.addView(sectionLabel("ABOUT & COPYRIGHT"), matchWidth)
        root.addView(
            body("Synvidra © Innotrepid. All rights reserved unless a separate license is granted. Third-party notices in NOTICE.md."),
            matchWidth,
        )

        return root
    }

    fun buildScrollable(
        context: Context,
        density: Float,
        onResetProcessingToNatural: () -> Unit,
        onThemeChanged: (mode: Int) -> Unit = {},
    ): ScrollView {
        val scroll = ScrollView(context)
        scroll.addView(build(context, density, onResetProcessingToNatural, onThemeChanged))
        return scroll
    }
}
