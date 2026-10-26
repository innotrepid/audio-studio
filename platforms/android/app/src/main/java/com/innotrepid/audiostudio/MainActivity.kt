package com.innotrepid.audiostudio

import android.Manifest
import android.app.Activity
import android.content.pm.PackageManager
import android.graphics.Color
import android.os.Bundle
import android.os.Environment
import android.view.Gravity
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import java.io.File

class MainActivity : Activity() {
    private external fun nativeEngineStatus(): String
    private external fun nativeStartRecording(outputPath: String): Boolean
    private external fun nativeStopRecording(): String

    private lateinit var statusText: TextView
    private lateinit var startButton: Button
    private lateinit var stopButton: Button
    private var recording = false
    private var startAfterPermission = false

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

        statusText = TextView(this).apply {
            text = nativeEngineStatus()
            textSize = 15f
            setTextColor(Color.rgb(180, 220, 190))
            gravity = Gravity.CENTER
        }

        startButton = Button(this).apply {
            text = "Start recording"
            setOnClickListener { requestPermissionOrStart() }
        }
        stopButton = Button(this).apply {
            text = "Stop and save WAV"
            isEnabled = false
            setOnClickListener { stopRecording() }
        }

        val notice = TextView(this).apply {
            text = "Recordings are saved as PCM16 WAV files in this app's private Music/Recordings folder. Keep the app open while recording. Noise reduction and editing are not implemented yet."
            textSize = 14f
            setTextColor(Color.rgb(190, 198, 214))
            gravity = Gravity.CENTER
            setPadding(0, 20, 0, 0)
        }

        root.addView(title, matchWidth())
        root.addView(subtitle, matchWidth())
        root.addView(statusText, matchWidth())
        root.addView(startButton, matchWidth())
        root.addView(stopButton, matchWidth())
        root.addView(notice, matchWidth())
        setContentView(root)
    }

    private fun requestPermissionOrStart() {
        if (checkSelfPermission(Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) {
            startRecording()
        } else {
            startAfterPermission = true
            requestPermissions(arrayOf(Manifest.permission.RECORD_AUDIO), REQUEST_RECORD_AUDIO)
        }
    }

    private fun startRecording() {
        val musicDirectory = getExternalFilesDir(Environment.DIRECTORY_MUSIC) ?: filesDir
        val recordingDirectory = File(musicDirectory, "Recordings")
        if (!recordingDirectory.exists() && !recordingDirectory.mkdirs()) {
            statusText.text = "Could not create the recordings folder."
            return
        }

        val output = File(recordingDirectory, "recording-${System.currentTimeMillis()}.wav")
        val started = try {
            nativeStartRecording(output.absolutePath)
        } catch (exception: Exception) {
            statusText.text = "Could not start recording: ${exception.message ?: "unknown error"}"
            false
        }

        recording = started
        startButton.isEnabled = !started
        stopButton.isEnabled = started
        statusText.text = if (started) {
            "Recording to ${output.name}. Tap Stop and save WAV when finished."
        } else {
            "Microphone did not start. Check permission and microphone availability."
        }
    }

    private fun stopRecording() {
        if (!recording) return
        recording = false
        startButton.isEnabled = true
        stopButton.isEnabled = false
        statusText.text = try {
            nativeStopRecording()
        } catch (exception: Exception) {
            "Could not finalize recording: ${exception.message ?: "unknown error"}"
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode != REQUEST_RECORD_AUDIO) return
        val shouldStart = startAfterPermission
        startAfterPermission = false
        if (shouldStart && grantResults.isNotEmpty() &&
            grantResults[0] == PackageManager.PERMISSION_GRANTED
        ) {
            startRecording()
        } else if (shouldStart) {
            statusText.text = "Microphone permission is required to record."
        }
    }

    override fun onDestroy() {
        if (recording) {
            recording = false
            nativeStopRecording()
        }
        super.onDestroy()
    }

    private fun matchWidth(): LinearLayout.LayoutParams =
        LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT
        )

    companion object {
        private const val REQUEST_RECORD_AUDIO = 1001
    }
}
