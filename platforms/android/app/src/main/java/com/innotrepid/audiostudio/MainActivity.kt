package com.innotrepid.audiostudio

import android.Manifest
import android.app.Activity
import android.content.ContentValues
import android.content.Intent
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
import java.io.FileInputStream
import java.io.IOException
import java.nio.ByteBuffer
import java.nio.ByteOrder
import android.os.Handler
import android.os.Looper
import android.os.SystemClock

class MainActivity : Activity() {
    private external fun nativeEngineStatus(): String
    private external fun nativeStartRecording(outputPath: String): Boolean
    private external fun nativeStopRecording(): String
    private external fun nativeProcessNoiseGate(inputPath: String, outputPath: String): String

    private lateinit var statusText: TextView
    private lateinit var startButton: Button
    private lateinit var stopButton: Button
    private lateinit var processButton: Button
    @Volatile private var processing = false
    private var recording = false
    private var startAfterPermission = false
    private var currentRecordingFile: File? = null
    private val mainHandler = Handler(Looper.getMainLooper())
    private var recordingStartedAtMs = 0L
    private val recordingClock = object : Runnable {
        override fun run() {
            if (!recording) return
            val elapsedSeconds = ((SystemClock.elapsedRealtime() - recordingStartedAtMs) / 1000L).coerceAtLeast(0L)
            val minutes = elapsedSeconds / 60L
            val seconds = elapsedSeconds % 60L
            statusText.text = "Recording · %02d:%02d".format(minutes, seconds)
            mainHandler.postDelayed(this, 1000L)
        }
    }

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
        processButton = Button(this).apply {
            text = "Reduce background noise in a WAV"
            setOnClickListener { chooseWavToProcess() }
        }

        val notice = TextView(this).apply {
            text = "Recordings are exported to Music/Audio Studio/Recordings. Choose a PCM16 WAV to create a separate noise-gated copy in Music/Audio Studio/Processed. This conservative gate reduces quieter noise between phrases; it is not AI noise cancellation or echo removal."
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
        root.addView(processButton, matchWidth())
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
        if (!started) output.delete()
        startButton.isEnabled = !started
        stopButton.isEnabled = started
        if (started) {
            recordingStartedAtMs = SystemClock.elapsedRealtime()
            statusText.text = "Recording · 00:00"
            mainHandler.removeCallbacks(recordingClock)
            mainHandler.post(recordingClock)
        } else {
            mainHandler.removeCallbacks(recordingClock)
            statusText.text = "Microphone did not start. Check permission and microphone availability, then try again."
        }
    }

    private fun stopRecording(interrupted: Boolean = false) {
        if (!recording) return
        recording = false
        mainHandler.removeCallbacks(recordingClock)
        startButton.isEnabled = true
        stopButton.isEnabled = false

        val nativeStatus = try {
            nativeStopRecording()
        } catch (exception: Exception) {
            "ERROR: Could not finalize recording: ${exception.message ?: "unknown error"}"
        }

        val interruptionNotice = if (interrupted) {
            "Recording was stopped because Audio Studio left the foreground.\n\n"
        } else {
            ""
        }
        val stagedFile = currentRecordingFile
        currentRecordingFile = null
        if (!nativeStatus.startsWith("Saved recording:")) {
            statusText.text = interruptionNotice + nativeStatus +
                "\n\nThe staged file, if present, was retained in app-private storage for recovery."
            return
        }
        if (stagedFile == null || !isCompletePcmWav(stagedFile)) {
            statusText.text = interruptionNotice +
                "Recording stopped, but WAV validation failed. The staged file was retained in app-private storage."
            return
        }

        statusText.text = try {
            val publicUri = publishRecording(stagedFile)
            if (publicUri != null) {
                if (!stagedFile.delete()) {
                    interruptionNotice + "Recording exported successfully, but the private staging copy could not be removed."
                } else {
                    interruptionNotice + "Saved and verified WAV in Music/Audio Studio/Recordings."
                }
            } else {
                interruptionNotice + "This Android version does not support automatic public export. The verified WAV remains in app-private storage."
            }
        } catch (exception: Exception) {
            interruptionNotice + "Export failed: ${exception.message ?: "unknown error"}. The verified WAV remains in app-private storage."
        }
    }

    private fun isCompletePcmWav(file: File): Boolean {
        if (!file.isFile || file.length() <= WAV_HEADER_BYTES) return false
        return try {
            val header = ByteArray(WAV_HEADER_BYTES.toInt())
            FileInputStream(file).use { input ->
                val data = java.io.DataInputStream(input)
                data.readFully(header)
            }
            if (String(header, 0, 4, Charsets.US_ASCII) != "RIFF" ||
                String(header, 8, 4, Charsets.US_ASCII) != "WAVE" ||
                String(header, 12, 4, Charsets.US_ASCII) != "fmt " ||
                String(header, 36, 4, Charsets.US_ASCII) != "data"
            ) return false

            val buffer = ByteBuffer.wrap(header).order(ByteOrder.LITTLE_ENDIAN)
            val riffSize = Integer.toUnsignedLong(buffer.getInt(4))
            val audioFormat = buffer.getShort(20).toInt()
            val channels = buffer.getShort(22).toInt()
            val sampleRate = Integer.toUnsignedLong(buffer.getInt(24))
            val bitsPerSample = buffer.getShort(34).toInt()
            val dataSize = Integer.toUnsignedLong(buffer.getInt(40))
            riffSize == file.length() - 8L && dataSize == file.length() - WAV_HEADER_BYTES &&
                audioFormat == 1 && (channels == 1 || channels == 2) && sampleRate > 0L &&
                bitsPerSample == 16 && dataSize > 0L && dataSize % (channels * 2L) == 0L
        } catch (_: Exception) {
            false
        }
    }


    private fun chooseWavToProcess() {
        if (recording || processing) {
            statusText.text = "Stop the current operation before processing a WAV."
            return
        }
        val picker = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "audio/*"
        }
        startActivityForResult(picker, REQUEST_PROCESS_WAV)
    }

    @Deprecated("Uses the platform document picker result API for broad Android compatibility")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode != REQUEST_PROCESS_WAV || resultCode != RESULT_OK) return
        val uri = data?.data ?: run {
            statusText.text = "No WAV file was selected."
            return
        }
        processButton.isEnabled = false
        processing = true
        statusText.text = "Preparing WAV for noise reduction…"
        Thread {
            val stagingDirectory = File(filesDir, "processing-staging")
            var input: File? = null
            var output: File? = null
            try {
                if (!stagingDirectory.exists() && !stagingDirectory.mkdirs()) {
                    throw IOException("Could not create processing staging folder.")
                }
                val inputFile = File(stagingDirectory, "source-${System.currentTimeMillis()}.wav")
                input = inputFile
                contentResolver.openInputStream(uri)?.use { source ->
                    inputFile.outputStream().use { destination -> source.copyTo(destination) }
                } ?: throw IOException("Could not open the selected document.")
                if (inputFile.length() <= WAV_HEADER_BYTES) {
                    throw IOException("The selected file is empty or too small to be a WAV.")
                }
                if (inputFile.length() > MAX_PROCESSING_INPUT_BYTES) {
                    throw IOException("This version supports WAV files up to 64 MiB.")
                }
                val outputFile = File(stagingDirectory, "processed-${System.currentTimeMillis()}.wav")
                output = outputFile
                val nativeStatus = nativeProcessNoiseGate(inputFile.absolutePath, outputFile.absolutePath)
                if (!nativeStatus.startsWith("Processed WAV:")) {
                    throw IOException(nativeStatus.removePrefix("ERROR:").trim())
                }
                if (!isCompletePcmWav(outputFile)) {
                    throw IOException("The processed file did not pass WAV integrity checks.")
                }
                val publicUri = publishRecording(outputFile, "Processed")
                    ?: throw IOException("Public export requires Android 10 or later. The processed file remains in app-private storage.")
                val outputName = outputFile.name
                val removed = outputFile.delete()
                mainHandler.post {
                    if (!isFinishing && !isDestroyed) {
                        statusText.text = if (removed) {
                            "Noise-reduced copy saved to Music/Audio Studio/Processed as $outputName. Your original is unchanged."
                        } else {
                            "Processed copy exported. A private staging copy could not be removed."
                        }
                    }
                }
            } catch (exception: Exception) {
                mainHandler.post {
                    if (!isFinishing && !isDestroyed) {
                        statusText.text = "Could not process WAV: ${exception.message ?: "unknown error"}. The original was not modified."
                    }
                }
            } finally {
                input?.delete()
                processing = false
                mainHandler.post {
                    if (!isFinishing && !isDestroyed) processButton.isEnabled = true
                }
            }
        }.start()
    }

    @Throws(IOException::class)
    private fun publishRecording(source: File, folder: String = "Recordings"): Uri? {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return null

        val values = ContentValues().apply {
            put(MediaStore.Audio.Media.DISPLAY_NAME, source.name)
            put(MediaStore.Audio.Media.MIME_TYPE, "audio/wav")
            put(
                MediaStore.Audio.Media.RELATIVE_PATH,
                Environment.DIRECTORY_MUSIC + "/Audio Studio/" + folder
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

    override fun onStop() {
        if (recording) stopRecording(interrupted = true)
        super.onStop()
    }

    override fun onDestroy() {
        if (recording) stopRecording(interrupted = true)
        mainHandler.removeCallbacks(recordingClock)
        super.onDestroy()
    }

    private fun matchWidth(): LinearLayout.LayoutParams =
        LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT
        )

    companion object {
        private const val REQUEST_RECORD_AUDIO = 1001
        private const val REQUEST_PROCESS_WAV = 1002
        private const val WAV_HEADER_BYTES = 44L
        private const val MAX_PROCESSING_INPUT_BYTES = 64L * 1024L * 1024L
    }
}
