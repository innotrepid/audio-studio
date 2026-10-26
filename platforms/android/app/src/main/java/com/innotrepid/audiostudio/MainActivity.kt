package com.innotrepid.audiostudio

import android.Manifest
import android.app.Activity
import android.content.ContentValues
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Color
import android.net.Uri
import android.media.MediaPlayer
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.provider.MediaStore
import android.view.Gravity
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.SeekBar
import android.widget.TextView
import java.io.File
import java.io.FileInputStream
import java.io.IOException
import java.nio.ByteBuffer
import java.nio.ByteOrder
import android.os.Handler
import android.os.Looper
import android.os.SystemClock

private interface NoiseGateProgressCallback {
    fun onProgress(processedFrames: Long, totalFrames: Long)
}

class MainActivity : Activity() {
    private external fun nativeEngineStatus(): String
    private external fun nativeStartRecording(outputPath: String): Boolean
    private external fun nativeStopRecording(): String
    private external fun nativeProcessNoiseGate(inputPath: String, outputPath: String, thresholdDbfs: Float, attenuationDb: Float, callback: NoiseGateProgressCallback): String
    private external fun nativeCancelNoiseGateProcessing()
    private external fun nativePrepareNoiseGateProcessing()

    private lateinit var statusText: TextView
    private lateinit var startButton: Button
    private lateinit var stopButton: Button
    private lateinit var processButton: Button
    private lateinit var playOriginalButton: Button
    private lateinit var playProcessedButton: Button
    private lateinit var stopPlaybackButton: Button
    private lateinit var cancelProcessingButton: Button
    private lateinit var processingProgress: ProgressBar
    private var playback: MediaPlayer? = null
    @Volatile private var comparisonOriginalFile: File? = null
    @Volatile private var comparisonProcessedFile: File? = null
    private lateinit var thresholdLabel: TextView
    private lateinit var attenuationLabel: TextView
    private lateinit var thresholdSeekBar: SeekBar
    private lateinit var attenuationSeekBar: SeekBar
    @Volatile private var processing = false
    @Volatile private var cancelRequested = false
    // Prevent repeated picker launches and recording while a document is being selected.
    private var wavPickerActive = false
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
        cleanupStaleProcessingFiles()
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
        playOriginalButton = Button(this).apply {
            text = "Play original"
            isEnabled = false
            setOnClickListener { playComparisonFile(comparisonOriginalFile, "original") }
        }
        playProcessedButton = Button(this).apply {
            text = "Play noise-reduced copy"
            isEnabled = false
            setOnClickListener { playComparisonFile(comparisonProcessedFile, "noise-reduced") }
        }
        processingProgress = ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal).apply {
            max = 100
            progress = 0
            visibility = android.view.View.GONE
        }
        cancelProcessingButton = Button(this).apply {
            text = "Cancel processing"
            isEnabled = false
            setOnClickListener {
                if (processing) {
                    cancelRequested = true
                    try { nativeCancelNoiseGateProcessing() } catch (_: Exception) { }
                    text = "Cancelling…"
                    isEnabled = false
                    statusText.text = "Cancelling safely after the current audio block…"
                }
            }
        }
        stopPlaybackButton = Button(this).apply {
            text = "Stop playback"
            isEnabled = false
            setOnClickListener { stopComparisonPlayback() }
        }

        val gatePreferences = getSharedPreferences("noise_gate_preferences", MODE_PRIVATE)
        thresholdLabel = TextView(this).apply {
            textSize = 14f
            setTextColor(Color.rgb(220, 225, 238))
            gravity = Gravity.CENTER
        }
        thresholdSeekBar = SeekBar(this).apply {
            max = 50
            progress = gatePreferences.getInt("threshold_progress", 22).coerceIn(0, max)
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                    thresholdLabel.text = "Noise threshold: ${progress - 70} dBFS"
                    if (fromUser) gatePreferences.edit().putInt("threshold_progress", progress).apply()
                }
                override fun onStartTrackingTouch(seekBar: SeekBar?) = Unit
                override fun onStopTrackingTouch(seekBar: SeekBar?) = Unit
            })
        }
        attenuationLabel = TextView(this).apply {
            textSize = 14f
            setTextColor(Color.rgb(220, 225, 238))
            gravity = Gravity.CENTER
        }
        attenuationSeekBar = SeekBar(this).apply {
            max = 36
            progress = gatePreferences.getInt("attenuation_progress", 18).coerceIn(0, max)
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                    attenuationLabel.text = "Noise reduction strength: ${progress - 36} dB"
                    if (fromUser) gatePreferences.edit().putInt("attenuation_progress", progress).apply()
                }
                override fun onStartTrackingTouch(seekBar: SeekBar?) = Unit
                override fun onStopTrackingTouch(seekBar: SeekBar?) = Unit
            })
        }
        thresholdLabel.text = "Noise threshold: ${thresholdSeekBar.progress - 70} dBFS"
        attenuationLabel.text = "Noise reduction strength: ${attenuationSeekBar.progress - 36} dB"

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
        root.addView(processingProgress, matchWidth())
        root.addView(cancelProcessingButton, matchWidth())
        root.addView(playOriginalButton, matchWidth())
        root.addView(playProcessedButton, matchWidth())
        root.addView(stopPlaybackButton, matchWidth())
        root.addView(thresholdLabel, matchWidth())
        root.addView(thresholdSeekBar, matchWidth())
        root.addView(attenuationLabel, matchWidth())
        root.addView(attenuationSeekBar, matchWidth())
        root.addView(notice, matchWidth())
        setContentView(root)
    }

    private fun requestPermissionOrStart() {
        if (recording || processing || wavPickerActive || startAfterPermission) {
            statusText.text = "Finish the current operation before recording."
            return
        }
        if (checkSelfPermission(Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) {
            startRecording()
        } else {
            startAfterPermission = true
            processButton.isEnabled = false
            startButton.isEnabled = false
            try {
                requestPermissions(arrayOf(Manifest.permission.RECORD_AUDIO), REQUEST_RECORD_AUDIO)
            } catch (exception: Exception) {
                startAfterPermission = false
                processButton.isEnabled = true
                startButton.isEnabled = true
                statusText.text = "Could not request microphone permission: ${exception.message ?: "unknown error"}"
            }
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
        processButton.isEnabled = !started
        stopButton.isEnabled = started
        if (started) {
            recordingStartedAtMs = SystemClock.elapsedRealtime()
            statusText.text = "Recording · 00:00"
            mainHandler.removeCallbacks(recordingClock)
            mainHandler.post(recordingClock)
        } else {
            mainHandler.removeCallbacks(recordingClock)
            processButton.isEnabled = !processing && !wavPickerActive && !startAfterPermission
            statusText.text = "Microphone did not start. Check permission and microphone availability, then try again."
        }
    }

    private fun stopRecording(interrupted: Boolean = false) {
        if (!recording) return
        recording = false
        mainHandler.removeCallbacks(recordingClock)
        startButton.isEnabled = !processing && !wavPickerActive && !startAfterPermission
        processButton.isEnabled = !processing && !wavPickerActive && !startAfterPermission
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
        if (recording || processing || wavPickerActive || startAfterPermission) {
            statusText.text = "Stop the current operation before processing a WAV."
            return
        }
        val picker = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "audio/*"
        }
        wavPickerActive = true
        processButton.isEnabled = false
        startButton.isEnabled = false
        try {
            startActivityForResult(picker, REQUEST_PROCESS_WAV)
        } catch (exception: Exception) {
            wavPickerActive = false
            processButton.isEnabled = true
            startButton.isEnabled = true
            statusText.text = "Could not open the document picker: ${exception.message ?: "unknown error"}"
        }
    }

    @Deprecated("Uses the platform document picker result API for broad Android compatibility")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode != REQUEST_PROCESS_WAV) return
        wavPickerActive = false
        if (resultCode != RESULT_OK) {
            processButton.isEnabled = true
            startButton.isEnabled = true
            return
        }
        val uri = data?.data ?: run {
            statusText.text = "No WAV file was selected."
            processButton.isEnabled = true
            startButton.isEnabled = true
            return
        }
        stopComparisonPlayback()
        processButton.isEnabled = false
        startButton.isEnabled = false
        playOriginalButton.isEnabled = false
        playProcessedButton.isEnabled = false
        comparisonOriginalFile?.delete()
        comparisonProcessedFile?.delete()
        comparisonOriginalFile = null
        comparisonProcessedFile = null
        // Reset native cancellation before staging begins. Do not reset it in the
        // processing JNI call, or a late staging cancellation could be lost.
        nativePrepareNoiseGateProcessing()
        processing = true
        cancelRequested = false
        processingProgress.progress = 0
        processingProgress.visibility = android.view.View.VISIBLE
        cancelProcessingButton.isEnabled = true
        cancelProcessingButton.text = "Cancel processing"
        val thresholdDbfs = (thresholdSeekBar.progress - 70).toFloat()
        val attenuationDb = (attenuationSeekBar.progress - 36).toFloat()
        statusText.text = "Preparing WAV for noise reduction…"
        Thread {
            val stagingDirectory = File(filesDir, "processing-staging")
            var input: File? = null
            var output: File? = null
            var keepComparisonFiles = false
            try {
                if (!stagingDirectory.exists() && !stagingDirectory.mkdirs()) {
                    throw IOException("Could not create processing staging folder.")
                }
                val inputFile = File(stagingDirectory, "source-${System.currentTimeMillis()}.wav")
                input = inputFile
                contentResolver.openInputStream(uri)?.use { source ->
                    inputFile.outputStream().use { destination ->
                        val buffer = ByteArray(64 * 1024)
                        while (true) {
                            if (cancelRequested) throw IOException("Processing cancelled by user.")
                            val count = source.read(buffer)
                            if (count < 0) break
                            destination.write(buffer, 0, count)
                        }
                    }
                } ?: throw IOException("Could not open the selected document.")
                if (inputFile.length() <= WAV_HEADER_BYTES) {
                    throw IOException("The selected file is empty or too small to be a WAV.")
                }
                val outputFile = File(stagingDirectory, "processed-${System.currentTimeMillis()}.wav")
                output = outputFile
                if (cancelRequested) throw IOException("Processing cancelled by user.")
                val callback = object : NoiseGateProgressCallback {
                    override fun onProgress(processedFrames: Long, totalFrames: Long) {
                        val percent = if (totalFrames <= 0L) 0 else
                            ((processedFrames.coerceAtLeast(0L).toDouble() / totalFrames.toDouble()) * 100.0).toInt().coerceIn(0, 100)
                        mainHandler.post {
                            if (!isFinishing && !isDestroyed && processing) {
                                processingProgress.progress = percent
                                statusText.text = "Reducing background noise… $percent%"
                            }
                        }
                    }
                }
                val nativeStatus = nativeProcessNoiseGate(
                    inputFile.absolutePath,
                    outputFile.absolutePath,
                    thresholdDbfs,
                    attenuationDb,
                    callback
                )
                if (!nativeStatus.startsWith("Processed WAV:")) {
                    throw IOException(nativeStatus.removePrefix("ERROR:").trim())
                }
                if (!isCompletePcmWav(outputFile)) {
                    throw IOException("The processed file did not pass WAV integrity checks.")
                }
                if (cancelRequested) throw IOException("Processing cancelled by user.")
                val publicUri = publishRecording(outputFile, "Processed") { cancelRequested }
                    ?: throw IOException("Public export requires Android 10 or later. The processed file remains in app-private storage.")
                if (cancelRequested) {
                    contentResolver.delete(publicUri, null, null)
                    throw IOException("Processing cancelled by user.")
                }
                val outputName = outputFile.name
                comparisonOriginalFile = inputFile
                comparisonProcessedFile = outputFile
                keepComparisonFiles = true
                mainHandler.post {
                    if (!isFinishing && !isDestroyed) {
                        playOriginalButton.isEnabled = true
                        playProcessedButton.isEnabled = true
                        statusText.text = "Noise-reduced copy saved as " + outputName + ". Use Play original and Play noise-reduced copy to compare them. Your source is unchanged."
                    }
                }
            } catch (exception: Exception) {
                mainHandler.post {
                    if (!isFinishing && !isDestroyed) {
                        val message = exception.message ?: "unknown error"
                        statusText.text = if (cancelRequested || message.contains("cancelled", ignoreCase = true)) {
                            "Processing cancelled. The original was not modified."
                        } else {
                            "Could not process WAV: $message. The original was not modified."
                        }
                    }
                }
            } finally {
                if (!keepComparisonFiles) {
                    input?.delete()
                    output?.delete()
                }
                processing = false
                cancelRequested = false
                mainHandler.post {
                    if (!isFinishing && !isDestroyed) {
                        processButton.isEnabled = true
                        startButton.isEnabled = true
                        cancelProcessingButton.isEnabled = false
                        cancelProcessingButton.text = "Cancel processing"
                        processingProgress.visibility = android.view.View.GONE
                    }
                }
            }
        }.start()
    }

    private fun playComparisonFile(file: File?, label: String) {
        if (recording || processing || wavPickerActive || startAfterPermission) {
            statusText.text = "Wait until the current operation finishes before playback."
            return
        }
        if (file == null || !file.isFile || file.length() <= WAV_HEADER_BYTES) {
            statusText.text = "No comparison file is available. Process a WAV first."
            return
        }
        stopComparisonPlayback()
        try {
            val player = MediaPlayer()
            playback = player
            player.setDataSource(file.absolutePath)
            player.setOnPreparedListener { prepared ->
                if (playback === prepared && !isFinishing && !isDestroyed) {
                    prepared.start()
                    stopPlaybackButton.isEnabled = true
                    statusText.text = "Playing " + label + " recording. You can switch to the other version at any time."
                } else {
                    prepared.release()
                }
            }
            player.setOnCompletionListener { completed ->
                if (playback === completed) {
                    completed.release()
                    playback = null
                    stopPlaybackButton.isEnabled = false
                    statusText.text = "Playback finished. Compare the other version or process another WAV."
                }
            }
            player.setOnErrorListener { failed, _, _ ->
                if (playback === failed) {
                    failed.release()
                    playback = null
                }
                stopPlaybackButton.isEnabled = false
                statusText.text = "Playback failed for the " + label + " file. Try another PCM16 WAV."
                true
            }
            stopPlaybackButton.isEnabled = true
            statusText.text = "Preparing " + label + " playback…"
            player.prepareAsync()
        } catch (exception: Exception) {
            playback?.release()
            playback = null
            stopPlaybackButton.isEnabled = false
            statusText.text = "Could not play " + label + " recording: " + (exception.message ?: "unknown error")
        }
    }

    private fun stopComparisonPlayback() {
        val current = playback
        playback = null
        if (current != null) {
            try {
                if (current.isPlaying) current.stop()
            } catch (_: IllegalStateException) {
                // The player may still be preparing or may already have stopped.
            } finally {
                current.release()
            }
        }
        if (::stopPlaybackButton.isInitialized) stopPlaybackButton.isEnabled = false
    }

    @Throws(IOException::class)
    private fun publishRecording(
        source: File,
        folder: String = "Recordings",
        isCancelled: (() -> Boolean)? = null
    ): Uri? {
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
            output.use { stream ->
                source.inputStream().use { input ->
                    val buffer = ByteArray(64 * 1024)
                    while (true) {
                        if (isCancelled?.invoke() == true) {
                            throw IOException("Export cancelled by user.")
                        }
                        val count = input.read(buffer)
                        if (count < 0) break
                        stream.write(buffer, 0, count)
                    }
                }
            }

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
        processButton.isEnabled = !recording && !processing && !wavPickerActive
        startButton.isEnabled = !recording && !processing && !wavPickerActive
        if (shouldStart && grantResults.isNotEmpty() &&
            grantResults[0] == PackageManager.PERMISSION_GRANTED
        ) {
            if (recording || processing || wavPickerActive) {
                statusText.text = "Permission granted, but another operation is active. Tap Start recording when it finishes."
            } else {
                startRecording()
            }
        } else if (shouldStart) {
            statusText.text = "Microphone permission is required to record."
        }
    }

    override fun onStop() {
        // Comparison playback is a foreground-only preview; release it when the
        // Activity loses the foreground (including while the document picker opens).
        stopComparisonPlayback()
        if (recording) stopRecording(interrupted = true)
        // Processing belongs to this Activity. Cancel it when leaving the foreground;
        // the worker will clean up staging files and any partial export.
        if (processing) {
            cancelRequested = true
            try { nativeCancelNoiseGateProcessing() } catch (_: Exception) { }
        }
        super.onStop()
    }

    override fun onDestroy() {
        // onStop normally runs first, but explicitly signal cancellation here as
        // a final lifecycle guard. The worker owns cleanup of its active files.
        if (processing) {
            cancelRequested = true
            try { nativeCancelNoiseGateProcessing() } catch (_: Exception) { }
        }
        if (recording) stopRecording(interrupted = true)
        stopComparisonPlayback()
        mainHandler.removeCallbacks(recordingClock)
        super.onDestroy()
    }

    /**
     * A process can be killed before the worker's finally block runs. Remove only
     * old processing scratch files on startup; never touch recordings or recent
     * files that may still belong to an active operation.
     */
    private fun cleanupStaleProcessingFiles() {
        val stagingDirectory = File(filesDir, "processing-staging")
        val cutoff = System.currentTimeMillis() - STALE_PROCESSING_FILE_AGE_MS
        val staleFiles = stagingDirectory.listFiles() ?: return
        staleFiles.forEach { file ->
            if (file.isFile &&
                (file.name.startsWith("source-") || file.name.startsWith("processed-")) &&
                file.lastModified() < cutoff
            ) {
                file.delete()
            }
        }
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
        private const val STALE_PROCESSING_FILE_AGE_MS = 24L * 60L * 60L * 1000L
    }
}
