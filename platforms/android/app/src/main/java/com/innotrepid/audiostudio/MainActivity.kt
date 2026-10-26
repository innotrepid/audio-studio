package com.innotrepid.audiostudio

import android.app.Activity
import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.TextView

class MainActivity : Activity() {
    private external fun nativeEngineStatus(): String

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        System.loadLibrary("audio_studio_android")

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setPadding(32, 32, 32, 32)
            setBackgroundColor(Color.rgb(16, 18, 24))
        }

        val title = TextView(this).apply {
            text = "Audio Studio"
            textSize = 32f
            setTextColor(Color.WHITE)
            gravity = Gravity.CENTER
        }

        val subtitle = TextView(this).apply {
            text = "Your voice. Your studio."
            textSize = 18f
            setTextColor(Color.rgb(190, 198, 214))
            gravity = Gravity.CENTER
            setPadding(0, 12, 0, 24)
        }

        val status = TextView(this).apply {
            text = nativeEngineStatus()
            textSize = 15f
            setTextColor(Color.rgb(180, 220, 190))
            gravity = Gravity.CENTER
        }

        val notice = TextView(this).apply {
            text = "Recording is not enabled yet. This build verifies the Android shell and native C++ bridge; microphone capture comes next."
            textSize = 14f
            setTextColor(Color.rgb(190, 198, 214))
            gravity = Gravity.CENTER
            setPadding(0, 24, 0, 0)
        }

        root.addView(title, matchWidth())
        root.addView(subtitle, matchWidth())
        root.addView(status, matchWidth())
        root.addView(notice, matchWidth())
        setContentView(root)
    }

    private fun matchWidth(): LinearLayout.LayoutParams =
        LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT
        )

    companion object {
        init {
            // Load occurs in onCreate so Android lifecycle errors are surfaced by the app.
        }
    }
}
