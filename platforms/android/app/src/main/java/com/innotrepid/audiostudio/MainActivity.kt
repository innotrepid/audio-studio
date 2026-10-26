package com.innotrepid.audiostudio

import android.Manifest
import android.app.Activity
import android.content.ContentValues
import android.content.pm.PackageManager
import android.graphics.Color
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.provider.MediaStore
import android.view.Gravity
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import java.io.File
import java.io.IOException

class MainActivity : Activity() {
    private external fun nativeEngineStatus(): String
    private external fun nativeStartRecording(outputPath: String): Boolean
    private external fun nativeStopRecording(): String

    private lateinit var statusText: TextView
    private lateinit var startButton: Button
    private lateinit var stopButton: Button
    private var recording = false
    private var startAfterPermission = false
    private var currentRecordingFile: File? = null

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
            text = "Recordings are exported to Music/Audio Studio/Recordings so you can find them outside the app. Keep the app open while recording. Noise reduction and editing are not implemented yet."
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
        val recordingDirectory = File(filesDir, "recording-staging")
        if (!recordingDirectory.exists() && !recordingDirectory.mkdirs()) {
            statusText.text = "Could not create a temporary recordings folder."
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
        currentRecordingFile = if (started) output else null
        startButton.isEnabled = !started
        stopButton.isEnabled = started
        statusText.text = if (started) {
            "Recording ${output.name}. Tap Stop and save WAV when finished."
        } else {
            "Microphone did not start. Check permission and microphone availability."
        }
    }

    private fun stopRecording() {
        if (!recording) return
        recording = false
        startButton.isEnabled = true
        stopButton.isEnabled = false

        val nativeStatus = try {
            nativeStopRecording()
        } catch (exception: Exception) {
            "Could not finalize recording: ${exception.message ?: "unknown error"}"
        }

        val stagedFile = currentRecordingFile
        currentRecordingFile = null
        statusText.text = if (stagedFile != null && stagedFile.isFile && stagedFile.length() > WAV_HEADER_BYTES) {
            try {
                val publicUri = publishRecording(stagedFile)
                if (publicUri != null) {
                    stagedFile.delete()
                    "$nativeStatus\n\nExported to Music/Audio Studio/Recordings. You can open it with your Files app or a music/audio app."
                } else {
                    "$nativeStatus\n\nThis Android version does not support automatic public export. The WAV remains in app-private storage."
                }
            } catch (exception: Exception) {
                "$nativeStatus\n\nExport failed: ${exception.message ?: "unknown error"}. The WAV remains in app-private storage."
            }
        } else {
            nativeStatus
        }
    }

    @Throws(IOException::class)
    private fun publishRecording(source: File): Uri? {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return null

        val values = ContentValues().apply {
            put(MediaStore.Audio.Media.DISPLAY_NAME, source.name)
            put(MediaStore.Audio.Media.MIME_TYPE, "audio/wav")
            put(
                MediaStore.Audio.Media.RELATIVE_PATH,
                Environment.DIRECTORY_MUSIC + "/Audio Studio/Recordings"
            )
            put(MediaStore.Audio.Media.IS_PENDING, 1)
        }

        val collection = MediaStore.Audio.Media.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY)
        val destination = contentResolver.insert(collection, values)
            ?: throw IOException("Android could not create the public WAV entry")

        try {
            val output = contentResolver.openOutputStream(destination, "w")
                ?: throw IOException("Android could not open the exported WAV")
            output.use { stream -> source.inputStream().use { input -> input.copyTo(stream) } }

            val completedValues = ContentValues().apply {
                put(MediaStore.Audio.Media.IS_PENDING, 0)
            }
            if (contentResolver.update(destination, completedValues, null, null) <= 0) {
                throw IOException("Android could not finalize the exported WAV")
            }
            return destination
        } catch (exception: Exception) {
            contentResolver.delete(destination, null, null)
            throw exception
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
        private const val WAV_HEADER_BYTES = 44L
    }
}
