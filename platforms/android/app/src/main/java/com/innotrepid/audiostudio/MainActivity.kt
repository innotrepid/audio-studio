package com.innotrepid.audiostudio

import android.Manifest
import android.app.Activity
import android.app.AlertDialog
import android.content.ContentValues
import android.content.ContentUris
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
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.SeekBar
import android.widget.ScrollView
import android.widget.TextView
import java.io.File
import java.io.FileInputStream
import java.io.IOException
import java.util.UUID
import java.nio.ByteBuffer
import java.nio.ByteOrder
import android.os.Handler
import android.os.Looper
import android.os.SystemClock

private interface NoiseGateProgressCallback {
    fun onProgress(processedFrames: Long, totalFrames: Long)
}

class MainActivity : Activity() {
    private fun uniqueWavName(prefix: String): String = "$prefix-${UUID.randomUUID()}.wav"

    private external fun nativeEngineStatus(): String
    private external fun nativeStartRecording(outputPath: String): Boolean
    private external fun nativeRecoverPartialWav(partialPath: String, outputPath: String): String
    private external fun nativeAnalyzeWavWaveform(inputPath: String, bucketCount: Int): FloatArray
    private external fun nativeWavAnalysisError(inputPath: String): String
    private external fun nativeGetWavMetadata(inputPath: String): LongArray
    private external fun nativeTrimWav(inputPath: String, outputPath: String, startFrame: Long, endFrame: Long): String
    private external fun nativeStopRecording(): String
    private external fun nativeRecordingMeter(): FloatArray
    private external fun nativeProcessNoiseGate(inputPath: String, outputPath: String, thresholdDbfs: Float, attenuationDb: Float, gainDb: Float, highPassHz: Float, callback: NoiseGateProgressCallback): String
    private external fun nativeCancelNoiseGateProcessing()
    private external fun nativePrepareNoiseGateProcessing()

    private lateinit var diagnosticsCapture: DiagnosticsCapture
    private lateinit var statusText: TextView
    private lateinit var processingStatus: TextView
    private lateinit var inputMeterLabel: TextView
    private lateinit var inputMeter: ProgressBar
    private lateinit var startButton: Button
    private lateinit var stopButton: Button
    private lateinit var processButton: Button
    private lateinit var waveformButton: Button
    private lateinit var waveformStatus: TextView
    private lateinit var waveformView: WaveformView
    private lateinit var trimStartLabel: TextView
    private lateinit var trimEndLabel: TextView
    private lateinit var trimSelectionSummary: TextView
    private lateinit var trimStartTimeInput: EditText
    private lateinit var trimEndTimeInput: EditText
    private lateinit var applyTrimTimecodesButton: Button
    private var exactTrimStartFrame: Long? = null
    private var exactTrimEndFrame: Long? = null
    private var applyingTrimTimecodes = false
    private lateinit var previewSelectionButton: Button
    private lateinit var trimStartSeekBar: SeekBar
    private lateinit var trimEndSeekBar: SeekBar
    private lateinit var trimWaveformButton: Button
    @Volatile private var waveformSourceFile: File? = null
    @Volatile private var waveformTotalFrames: Long = 0L
    @Volatile private var waveformSampleRate: Long = 0L
    private lateinit var recoverButton: Button
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
    private lateinit var dspGainLabel: TextView
    private lateinit var highPassLabel: TextView
    private lateinit var dspGainSeekBar: SeekBar
    private lateinit var highPassSeekBar: SeekBar
    @Volatile private var processing = false
    @Volatile private var cancelRequested = false
    // Prevent repeated picker launches and recording while a document is being selected.
    private var wavPickerActive = false
    private var recording = false
    private var startAfterPermission = false
    private var currentRecordingFile: File? = null
    private val mainHandler = Handler(Looper.getMainLooper())
    private var trimPreviewEndMs = -1
    private val trimPreviewClock = object : Runnable {
        override fun run() {
            val player = playback
            if (player == null || trimPreviewEndMs < 0 || !player.isPlaying) return
            if (player.currentPosition >= trimPreviewEndMs) {
                stopComparisonPlayback()
                statusText.text = "Selection preview finished. Adjust the range or export the trim."
            } else {
                mainHandler.postDelayed(this, 25L)
            }
        }
    }
    private var recordingStartedAtMs = 0L
    private val recordingMeterClock = object : Runnable {
        override fun run() {
            if (!recording || isFinishing || isDestroyed) return
            val values = try { nativeRecordingMeter() } catch (_: Exception) { floatArrayOf() }
            if (values.size >= 3) {
                val rms = values[0].coerceIn(0.0f, 1.0f)
                val peak = values[1].coerceIn(0.0f, 1.0f)
                val clipped = values[2] >= 0.5f
                inputMeter.progress = (peak * inputMeter.max).toInt()
                val peakDbfs = if (peak > 0.000001f) (20.0 * kotlin.math.log10(peak.toDouble())).toInt() else -120
                val rmsDbfs = if (rms > 0.000001f) (20.0 * kotlin.math.log10(rms.toDouble())).toInt() else -120
                inputMeterLabel.text = if (clipped) {
                    "Input: RMS $rmsDbfs dBFS · Peak $peakDbfs dBFS — CLIPPING DETECTED; lower input gain"
                } else {
                    "Input: RMS $rmsDbfs dBFS · Peak $peakDbfs dBFS"
                }
                inputMeterLabel.setTextColor(if (clipped) Color.rgb(255, 120, 110) else Color.rgb(190, 220, 200))
            }
            mainHandler.postDelayed(this, 100L)
        }
    }
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
        diagnosticsCapture = DiagnosticsCapture(this)
        diagnosticsCapture.installUncaughtExceptionCapture()
        diagnosticsCapture.record("app_started", "success", mapOf("android_release" to Build.VERSION.RELEASE))
        cleanupStaleProcessingFiles()
        System.loadLibrary("audio_studio_android")

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.TOP or Gravity.CENTER_HORIZONTAL
            setPadding(20, 24, 20, 24)
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

        inputMeterLabel = TextView(this).apply {
            text = "Input level appears while recording"
            textSize = 13f
            setTextColor(Color.rgb(190, 198, 214))
            gravity = Gravity.CENTER
        }
        inputMeter = ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal).apply {
            max = 1000
            progress = 0
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
        processingStatus = TextView(this).apply {
            text = "Choose a PCM16 WAV to create a noise-reduced copy."
            textSize = 13f
            setTextColor(Color.rgb(190, 198, 214))
            gravity = Gravity.CENTER
            setPadding(0, dp(8), 0, dp(8))
        }
        waveformStatus = TextView(this).apply { text = "No waveform loaded."; textSize = 13f; setTextColor(Color.rgb(190, 198, 214)); gravity = Gravity.CENTER }
        waveformView = WaveformView(this)
        trimStartLabel = TextView(this).apply {
            text = "Trim start: 0%"
            textSize = 13f
            setTextColor(Color.rgb(190, 198, 214))
        }
        trimStartSeekBar = SeekBar(this).apply {
            max = 1000
            progress = 0
            setOnSeekBarChangeListener(trimRangeListener {
                if (!applyingTrimTimecodes) exactTrimStartFrame = null
                trimStartLabel.text = "Trim start: ${it / 10.0f}%"
                waveformView.setSelection(it / 1000f, trimEndSeekBar.progress / 1000f)
                updateTrimSelectionSummary()
            })
        }
        trimEndLabel = TextView(this).apply {
            text = "Trim end: 100%"
            textSize = 13f
            setTextColor(Color.rgb(190, 198, 214))
        }
        trimEndSeekBar = SeekBar(this).apply {
            max = 1000
            progress = 1000
            setOnSeekBarChangeListener(trimRangeListener {
                if (!applyingTrimTimecodes) exactTrimEndFrame = null
                trimEndLabel.text = "Trim end: ${it / 10.0f}%"
                waveformView.setSelection(trimStartSeekBar.progress / 1000f, it / 1000f)
                updateTrimSelectionSummary()
            })
        }
        waveformView.onTrimHandleMoved = { isStart, fraction ->
            val requested = (fraction * 1000f).toInt().coerceIn(0, 1000)
            if (isStart) {
                exactTrimStartFrame = null
                trimStartSeekBar.progress = requested.coerceAtMost((trimEndSeekBar.progress - 1).coerceAtLeast(0))
            } else {
                exactTrimEndFrame = null
                trimEndSeekBar.progress = requested.coerceAtLeast((trimStartSeekBar.progress + 1).coerceAtMost(1000))
            }
            waveformView.setSelection(trimStartSeekBar.progress / 1000f, trimEndSeekBar.progress / 1000f)
            updateTrimSelectionSummary()
        }
        trimSelectionSummary = TextView(this).apply {
            text = "Selected duration: load a WAV"
            textSize = 13f
            setTextColor(Color.rgb(190, 198, 214))
        }

        trimStartTimeInput = EditText(this).apply {
            setSingleLine(true)
            hint = "In point · MM:SS.mmm"
            setText("00:00.000")
            setTextColor(Color.WHITE)
            setHintTextColor(Color.rgb(150, 158, 176))
            setSelectAllOnFocus(true)
            inputType = android.text.InputType.TYPE_CLASS_TEXT
        }
        trimEndTimeInput = EditText(this).apply {
            setSingleLine(true)
            hint = "Out point · MM:SS.mmm"
            setText("00:00.000")
            setTextColor(Color.WHITE)
            setHintTextColor(Color.rgb(150, 158, 176))
            setSelectAllOnFocus(true)
            inputType = android.text.InputType.TYPE_CLASS_TEXT
        }
        applyTrimTimecodesButton = Button(this).apply {
            text = "Apply exact in/out points"
            setOnClickListener { applyTrimTimecodes() }
        }
        previewSelectionButton = Button(this).apply {
            text = "Preview selected range"
            isEnabled = false
            setOnClickListener { previewTrimSelection() }
        }
        trimWaveformButton = Button(this).apply {
            text = "Trim selected range to new WAV"
            isEnabled = false
            setOnClickListener { chooseTrimOutput() }
        }
        waveformButton = Button(this).apply {
            text = "Load WAV waveform"
            setOnClickListener { chooseWavWaveform() }
        }
        recoverButton = Button(this).apply {
            text = "Check for interrupted recordings"
            setOnClickListener { showInterruptedRecordings() }
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
                    processingStatus.text = "Cancelling safely after the current audio block…"
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
                    }
                override fun onStartTrackingTouch(seekBar: SeekBar?) = Unit
                override fun onStopTrackingTouch(seekBar: SeekBar?) {
                    seekBar?.let { gatePreferences.edit().putInt("threshold_progress", it.progress).apply() }
                }
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
                    }
                override fun onStartTrackingTouch(seekBar: SeekBar?) = Unit
                override fun onStopTrackingTouch(seekBar: SeekBar?) {
                    seekBar?.let { gatePreferences.edit().putInt("attenuation_progress", it.progress).apply() }
                }
            })
        }
        thresholdLabel.text = "Noise threshold: ${thresholdSeekBar.progress - 70} dBFS"
        attenuationLabel.text = "Noise reduction strength: ${attenuationSeekBar.progress - 36} dB"

        dspGainLabel = TextView(this).apply {
            textSize = 14f
            setTextColor(Color.rgb(220, 225, 238))
            gravity = Gravity.CENTER
        }
        dspGainSeekBar = SeekBar(this).apply {
            max = 36
            progress = gatePreferences.getInt("dsp_gain_progress", 24).coerceIn(0, max)
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                    dspGainLabel.text = "Output gain: ${progress - 24} dB"
                    }
                override fun onStartTrackingTouch(seekBar: SeekBar?) = Unit
                override fun onStopTrackingTouch(seekBar: SeekBar?) {
                    seekBar?.let { gatePreferences.edit().putInt("dsp_gain_progress", it.progress).apply() }
                }
            })
        }
        highPassLabel = TextView(this).apply {
            textSize = 14f
            setTextColor(Color.rgb(220, 225, 238))
            gravity = Gravity.CENTER
        }
        highPassSeekBar = SeekBar(this).apply {
            max = 49
            progress = gatePreferences.getInt("high_pass_progress", 0).coerceIn(0, max)
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                    val cutoff = if (progress == 0) 0 else (20 + (progress - 1) * 10).coerceAtMost(500)
                    highPassLabel.text = if (cutoff == 0) "High-pass filter: Off" else "High-pass filter: ${cutoff} Hz"
                    }
                override fun onStartTrackingTouch(seekBar: SeekBar?) = Unit
                override fun onStopTrackingTouch(seekBar: SeekBar?) {
                    seekBar?.let { gatePreferences.edit().putInt("high_pass_progress", it.progress).apply() }
                }
            })
        }
        dspGainLabel.text = "Output gain: ${dspGainSeekBar.progress - 24} dB"
        val initialHighPass = if (highPassSeekBar.progress == 0) 0 else
            (20 + (highPassSeekBar.progress - 1) * 10).coerceAtMost(500)
        highPassLabel.text = if (initialHighPass == 0) "High-pass filter: Off" else "High-pass filter: ${initialHighPass} Hz"

        val presetRow = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER
        }
        listOf("Natural", "Voice clarity", "Rumble cut").forEach { preset ->
            presetRow.addView(Button(this).apply {
                text = preset
                setOnClickListener { applyProcessingPreset(preset) }
            }, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        }

        val notice = TextView(this).apply {
            text = "Recordings are exported to Music/Audio Studio/Recordings. Choose a PCM16 WAV to create a separate noise-gated copy in Music/Audio Studio/Processed. This conservative gate reduces quieter noise between phrases; it is not AI noise cancellation or echo removal."
            textSize = 14f
            setTextColor(Color.rgb(190, 198, 214))
            gravity = Gravity.CENTER
            setPadding(0, 20, 0, 0)
        }

        fun makeSection(heading: String, description: String): LinearLayout {
            val section = LinearLayout(this).apply {
                orientation = LinearLayout.VERTICAL
                setPadding(18, 18, 18, 18)
                background = android.graphics.drawable.GradientDrawable().apply {
                    setColor(Color.rgb(28, 34, 45))
                    cornerRadius = 22f
                }
            }
            section.addView(TextView(this).apply {
                text = heading
                textSize = 20f
                setTextColor(Color.WHITE)
                setTypeface(null, android.graphics.Typeface.BOLD)
            }, matchWidth())
            section.addView(TextView(this).apply {
                text = description
                textSize = 13f
                setTextColor(Color.rgb(190, 198, 214))
                setPadding(0, 4, 0, 12)
            }, matchWidth())
            return section
        }
        val recordSection = makeSection("Record", "Capture a clean take and watch your input level.")
        recordSection.addView(title, matchWidth())
        recordSection.addView(subtitle, matchWidth())
        recordSection.addView(statusText, matchWidth())
        recordSection.addView(inputMeterLabel, matchWidth())
        recordSection.addView(inputMeter, matchWidth())
        recordSection.addView(startButton, matchWidth())
        recordSection.addView(stopButton, matchWidth())
        recordSection.addView(recoverButton, matchWidth())

        val editSection = makeSection("Edit a recording", "Inspect the waveform, preview a range, and export a trim. Your source stays unchanged.")
        editSection.addView(waveformButton, matchWidth())
        editSection.addView(waveformView, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 180))
        editSection.addView(trimStartLabel, matchWidth())
        editSection.addView(trimStartSeekBar, matchWidth())
        editSection.addView(trimEndLabel, matchWidth())
        editSection.addView(trimEndSeekBar, matchWidth())
        editSection.addView(TextView(this).apply {
            text = "PRECISE TIME CODES · minutes:seconds.milliseconds"
            textSize = 12f
            setTextColor(Color.rgb(190, 198, 214))
            setPadding(0, 10, 0, 2)
        }, matchWidth())
        editSection.addView(trimStartTimeInput, matchWidth())
        editSection.addView(trimEndTimeInput, matchWidth())
        editSection.addView(applyTrimTimecodesButton, matchWidth())
        editSection.addView(trimSelectionSummary, matchWidth())
        editSection.addView(previewSelectionButton, matchWidth())
        editSection.addView(trimWaveformButton, matchWidth())
        editSection.addView(waveformStatus, matchWidth())

        val processingSection = makeSection("Clean up audio", "Choose a preset first. Advanced controls are available below; processing creates a separate copy.")
        processingSection.addView(processButton, matchWidth())
        processingSection.addView(processingStatus, matchWidth())
        processingSection.addView(presetRow, matchWidth())

        val advancedControls = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            visibility = ViewGroup.GONE
        }
        val advancedControlsToggle = Button(this).apply {
            text = "Show advanced controls"
            contentDescription = "Show or hide noise gate, gain, and high-pass settings"
            setOnClickListener {
                val show = advancedControls.visibility != ViewGroup.VISIBLE
                advancedControls.visibility = if (show) ViewGroup.VISIBLE else ViewGroup.GONE
                text = if (show) "Hide advanced controls" else "Show advanced controls"
            }
        }
        advancedControls.addView(thresholdLabel, matchWidth())
        advancedControls.addView(thresholdSeekBar, matchWidth())
        advancedControls.addView(attenuationLabel, matchWidth())
        advancedControls.addView(attenuationSeekBar, matchWidth())
        advancedControls.addView(dspGainLabel, matchWidth())
        advancedControls.addView(dspGainSeekBar, matchWidth())
        advancedControls.addView(highPassLabel, matchWidth())
        advancedControls.addView(highPassSeekBar, matchWidth())
        processingSection.addView(advancedControlsToggle, matchWidth())
        processingSection.addView(advancedControls, matchWidth())
        processingSection.addView(processingProgress, matchWidth())
        processingSection.addView(cancelProcessingButton, matchWidth())
        processingSection.addView(playOriginalButton, matchWidth())
        processingSection.addView(playProcessedButton, matchWidth())
        processingSection.addView(stopPlaybackButton, matchWidth())
        processingSection.addView(notice, matchWidth())

        // Browse WAVs exported by Audio Studio without reopening a document picker.
        val libraryStatus = TextView(this).apply {
            text = "Your Audio Studio WAV files will appear here."
            textSize = 14f
            setTextColor(Color.rgb(190, 198, 214))
            setPadding(0, 8, 0, 8)
        }
        val libraryItems = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        val librarySection = makeSection(
            "Your audio library",
            "Find recordings, processed copies, and trimmed exports. Editing and processing never overwrite the selected source."
        )
        fun refreshAudioLibrary() {
            libraryItems.removeAllViews()
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
                libraryStatus.text = "The in-app library requires Android 10 or later. Use the WAV picker on this device."
                return
            }
            libraryStatus.text = "Looking for Audio Studio WAV files…"
            diagnosticsCapture.record("library_scan", "started", mapOf("scope" to "Music/Audio Studio"))
            try {
                // Query Files rather than Audio.Media: WAVs are sometimes indexed with
                // audio/x-wav or a generic MIME type, so MIME filtering hides valid files.
                val collection = MediaStore.Files.getContentUri(MediaStore.VOLUME_EXTERNAL)
                val projection = arrayOf(
                    MediaStore.Files.FileColumns._ID,
                    MediaStore.MediaColumns.DISPLAY_NAME,
                    MediaStore.MediaColumns.RELATIVE_PATH,
                    MediaStore.MediaColumns.SIZE
                )
                val rows = mutableListOf<Triple<Uri, String, String>>()
                var scannedRows = 0
                contentResolver.query(
                    collection,
                    projection,
                    "${MediaStore.MediaColumns.RELATIVE_PATH} LIKE ?",
                    arrayOf("Music/Audio Studio/%"),
                    "${MediaStore.MediaColumns.DATE_ADDED} DESC"
                )?.use { cursor ->
                    val idColumn = cursor.getColumnIndexOrThrow(MediaStore.Files.FileColumns._ID)
                    val nameColumn = cursor.getColumnIndexOrThrow(MediaStore.MediaColumns.DISPLAY_NAME)
                    val pathColumn = cursor.getColumnIndexOrThrow(MediaStore.MediaColumns.RELATIVE_PATH)
                    val sizeColumn = cursor.getColumnIndexOrThrow(MediaStore.MediaColumns.SIZE)
                    while (cursor.moveToNext()) {
                        scannedRows++
                        val name = cursor.getString(nameColumn) ?: continue
                        if (!name.endsWith(".wav", ignoreCase = true)) continue
                        val folder = cursor.getString(pathColumn)?.substringAfter("Audio Studio/")?.trimEnd('/') ?: "Audio Studio"
                        val bytes = cursor.getLong(sizeColumn).coerceAtLeast(0L)
                        val sizeKb = (bytes / 1024L).coerceAtLeast(if (bytes > 0L) 1L else 0L)
                        val uri = ContentUris.withAppendedId(collection, cursor.getLong(idColumn))
                        rows.add(Triple(uri, name, folder + " · " + sizeKb + " KB" + if (bytes == 0L) " · EMPTY FILE" else ""))
                        if (rows.size >= 100) break
                    }
                } ?: throw IOException("Android could not read the audio library.")
                diagnosticsCapture.record("library_scan", "success", mapOf(
                    "indexed_rows_in_scope" to scannedRows,
                    "wav_rows_found" to rows.size
                ))
                if (rows.isEmpty()) {
                    libraryStatus.text = "No WAV files found in Music/Audio Studio yet. Record or export a WAV first."
                    return
                }
                libraryStatus.text = rows.size.toString() + " recent WAV file(s) · showing up to 100"
                rows.forEach { row ->
                    val uri = row.first
                    val name = row.second
                    val details = row.third
                    val item = LinearLayout(this).apply {
                        orientation = LinearLayout.VERTICAL
                        setPadding(12, 12, 12, 12)
                        background = android.graphics.drawable.GradientDrawable().apply {
                            setColor(Color.rgb(20, 20, 30))
                            cornerRadius = dp(12).toFloat()
                            setStroke(dp(1), Color.rgb(58, 54, 80))
                        }
                    }
                    item.addView(TextView(this).apply {
                        text = name
                        textSize = 15f
                        setTextColor(Color.WHITE)
                        setTypeface(null, android.graphics.Typeface.BOLD)
                    }, matchWidth())
                    item.addView(TextView(this).apply {
                        text = details
                        textSize = 12f
                        setTextColor(Color.rgb(190, 198, 214))
                        setPadding(0, dp(3), 0, dp(8))
                    }, matchWidth())
                    val actions = LinearLayout(this).apply {
                        orientation = LinearLayout.HORIZONTAL
                        gravity = Gravity.CENTER
                    }
                    actions.addView(Button(this).apply {
                        text = "Edit"
                        setOnClickListener {
                            librarySection.visibility = android.view.View.GONE
                            editSection.visibility = android.view.View.VISIBLE
                            recordSection.visibility = android.view.View.GONE
                            processingSection.visibility = android.view.View.GONE
                            analyzeSelectedWaveform(uri)
                        }
                    }, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
                    actions.addView(Button(this).apply {
                        text = "Process"
                        setOnClickListener {
                            librarySection.visibility = android.view.View.GONE
                            editSection.visibility = android.view.View.GONE
                            recordSection.visibility = android.view.View.GONE
                            processingSection.visibility = android.view.View.VISIBLE
                            onActivityResult(REQUEST_PROCESS_WAV, RESULT_OK, Intent().setData(uri))
                        }
                    }, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
                    item.addView(actions, matchWidth())
                    libraryItems.addView(item, LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
                    ).apply { bottomMargin = dp(10) })
                }
            } catch (exception: Exception) {
                diagnosticsCapture.record("library_scan", "failure", error = exception)
                libraryStatus.text = "Could not load the library: " + (exception.message ?: "unknown error") + ". You can still use the WAV picker."
            }
        }

        librarySection.addView(Button(this).apply {
            text = "Refresh audio library"
            setOnClickListener { refreshAudioLibrary() }
        }, matchWidth())
        librarySection.addView(Button(this).apply {
            text = "Export diagnostics report"
            setOnClickListener { chooseDiagnosticsExport() }
        }, matchWidth())
        librarySection.addView(libraryStatus, matchWidth())
        librarySection.addView(libraryItems, matchWidth())

        fun sectionLayoutParams() = LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
        ).apply { bottomMargin = 18 }
        recordSection.visibility = android.view.View.VISIBLE
        editSection.visibility = android.view.View.GONE
        processingSection.visibility = android.view.View.GONE
        librarySection.visibility = android.view.View.GONE
        root.addView(recordSection, sectionLayoutParams())
        root.addView(editSection, sectionLayoutParams())
        root.addView(processingSection, sectionLayoutParams())
        root.addView(librarySection, sectionLayoutParams())

        val navBar = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER
            setPadding(8, 8, 8, 8)
            setBackgroundColor(Color.rgb(28, 34, 45))
        }
        fun navButton(label: String, target: LinearLayout): Button = Button(this).apply {
            text = label
            isAllCaps = false
            setOnClickListener {
                recordSection.visibility = if (target === recordSection) android.view.View.VISIBLE else android.view.View.GONE
                editSection.visibility = if (target === editSection) android.view.View.VISIBLE else android.view.View.GONE
                processingSection.visibility = if (target === processingSection) android.view.View.VISIBLE else android.view.View.GONE
                librarySection.visibility = if (target === librarySection) android.view.View.VISIBLE else android.view.View.GONE
                if (target === librarySection) refreshAudioLibrary()

                // Navigation must be a single tap. Do not force an orientation change here:
                // Android can deliver it asynchronously and some devices recreate or relayout
                // the Activity while a picker is being launched. The existing ScrollView
                // handles portrait and landscape without rebuilding the view tree.
            }
        }
        listOf(
            navButton("Record", recordSection),
            navButton("Edit", editSection),
            navButton("Process", processingSection),
            navButton("Library", librarySection)
        ).forEach { navBar.addView(it, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)) }

        val screenScroll = ScrollView(this).apply {
            isFillViewport = true
            clipToPadding = false
            setBackgroundColor(Color.rgb(16, 18, 24))
            addView(root, ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            ))
        }
        val appShell = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(Color.rgb(16, 18, 24))
            addView(screenScroll, LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f
            ))
            addView(navBar, LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
            ))
        }
        // Apply the studio control language once, after the view tree is built.
        // This is construction-time styling: taps and gestures never rebuild the screen.
        fun styleStudioButtons(view: android.view.View) {
            if (view is Button) {
                view.isAllCaps = false
                view.textSize = 14f
                view.setPadding(dp(16), dp(10), dp(16), dp(10))
                view.minHeight = dp(46)
                view.stateListAnimator = null
                view.background = android.graphics.drawable.GradientDrawable().apply {
                    setColor(Color.rgb(34, 34, 48))
                    cornerRadius = dp(12).toFloat()
                    setStroke(dp(1), Color.rgb(58, 54, 80))
                }
                view.setTextColor(Color.rgb(243, 240, 255))
            } else if (view is ViewGroup) {
                for (index in 0 until view.childCount) styleStudioButtons(view.getChildAt(index))
            }
        }
        styleStudioButtons(appShell)

        fun emphasize(button: Button, fill: Int, text: Int = Color.rgb(16, 18, 24)) {
            button.background = android.graphics.drawable.GradientDrawable().apply {
                setColor(fill)
                cornerRadius = dp(12).toFloat()
            }
            button.setTextColor(text)
            button.typeface = android.graphics.Typeface.DEFAULT_BOLD
        }
        emphasize(startButton, Color.rgb(178, 155, 255))
        emphasize(processButton, Color.rgb(178, 155, 255))
        emphasize(trimWaveformButton, Color.rgb(178, 155, 255))
        emphasize(stopButton, Color.rgb(255, 122, 134))
        emphasize(cancelProcessingButton, Color.rgb(255, 122, 134))

        setContentView(appShell)
        root.post { showInterruptedRecordings(showWhenNone = false) }
    }

    private fun dp(value: Int): Int = (value * resources.displayMetrics.density + 0.5f).toInt()

    private fun applyProcessingPreset(preset: String) {
        val values = when (preset) {
            "Voice clarity" -> intArrayOf(22, 18, 26, 7) // -48 dBFS, -18 dB, +2 dB, 80 Hz
            "Rumble cut" -> intArrayOf(10, 24, 24, 11) // gentle gate, neutral gain, 120 Hz high-pass
            else -> intArrayOf(16, 24, 24, 0) // natural: -54 dBFS, -12 dB, neutral gain, no filter
        }
        thresholdSeekBar.progress = values[0]
        attenuationSeekBar.progress = values[1]
        dspGainSeekBar.progress = values[2]
        highPassSeekBar.progress = values[3]
        getSharedPreferences("noise_gate_preferences", MODE_PRIVATE).edit()
            .putInt("threshold_progress", values[0])
            .putInt("attenuation_progress", values[1])
            .putInt("dsp_gain_progress", values[2])
            .putInt("high_pass_progress", values[3])
            .apply()
        statusText.text = "${preset} preset applied. Adjust the sliders to taste; always A/B-check the result."
    }

    private fun showInterruptedRecordings(showWhenNone: Boolean = true) {
        if (recording || processing || wavPickerActive) {
            statusText.text = "Finish the current operation before recovering a recording."
            return
        }
        val directory = File(filesDir, "recording-staging")
        val candidates = directory.listFiles()?.filter { file ->
            file.isFile && file.name.matches(Regex(".+\\.wav\\.partial-[0-9]+"))
        }?.sortedBy { it.name } ?: emptyList()
        if (candidates.isEmpty()) {
            if (showWhenNone) statusText.text = "No interrupted recordings were found."
            return
        }
        AlertDialog.Builder(this)
            .setTitle("Interrupted recordings found")
            .setMessage("Choose a temporary recording to attempt recovery. Audio Studio validates the WAV before publishing it; invalid files are retained.")
            .setItems(candidates.map { it.name }.toTypedArray()) { _, index ->
                val partial = candidates[index]
                val output = File(partial.parentFile, uniqueWavName("recovered"))
                AlertDialog.Builder(this)
                    .setTitle("Recover ${partial.name}?")
                    .setMessage("Recovery will not replace an existing recording. If the audio is invalid, the temporary file will be left in place.")
                    .setNegativeButton("Not now", null)
                    .setPositiveButton("Recover") { _, _ -> recoverInterruptedRecording(partial, output) }
                    .show()
            }
            .setNegativeButton("Later", null)
            .show()
    }

    private fun recoverInterruptedRecording(partial: File, output: File) {
        recoverButton.isEnabled = false
        startButton.isEnabled = false
        processButton.isEnabled = false
        statusText.text = "Validating and recovering ${partial.name}…"
        Thread {
            var result = try {
                nativeRecoverPartialWav(partial.absolutePath, output.absolutePath)
            } catch (exception: Exception) {
                "ERROR: ${exception.message ?: "native recovery failed"}"
            }
            if (result == "RECOVERED") {
                result = try {
                    val publicUri = publishRecording(output)
                    if (publicUri != null) {
                        if (output.delete()) "Recovered and saved to Music/Audio Studio/Recordings."
                        else "Recovered and exported; the private recovered copy could not be removed."
                    } else {
                        "Recovered WAV remains in app-private storage: ${output.absolutePath}"
                    }
                } catch (exception: Exception) {
                    "Recovered WAV remains in app-private storage. Export failed: ${exception.message ?: "unknown error"}"
                }
            }
            val finalResult = result
            mainHandler.post {
                if (!isFinishing && !isDestroyed) {
                    statusText.text = finalResult
                    recoverButton.isEnabled = true
                    startButton.isEnabled = !recording && !processing && !wavPickerActive
                    processButton.isEnabled = !recording && !processing && !wavPickerActive
                    if (finalResult.startsWith("Recovered")) showInterruptedRecordings(showWhenNone = false)
                }
            }
        }.start()
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
        // Do not let comparison audio bleed into a new microphone recording.
        stopComparisonPlayback()
        val recordingDirectory = File(filesDir, "recording-staging")
        if (!recordingDirectory.exists() && !recordingDirectory.mkdirs()) {
            statusText.text = "Could not create a temporary recordings folder."
            return
        }

        val output = File(recordingDirectory, uniqueWavName("recording"))
        diagnosticsCapture.record("recording_start", "started", mapOf("staging_name" to output.name))
        val started = try {
            nativeStartRecording(output.absolutePath)
        } catch (exception: Exception) {
            diagnosticsCapture.record("recording_start", "failure", mapOf("staging_name" to output.name), exception)
            statusText.text = "Could not start recording: ${exception.message ?: "unknown error"}"
            false
        }
        diagnosticsCapture.record("recording_start", if (started) "success" else "failure",
            mapOf("staging_name" to output.name, "staging_exists" to output.exists(), "staging_bytes" to output.length()))

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
            mainHandler.removeCallbacks(recordingMeterClock)
            inputMeter.progress = 0
            inputMeterLabel.text = "Input level starting…"
            inputMeterLabel.setTextColor(Color.rgb(190, 220, 200))
            mainHandler.post(recordingClock)
            mainHandler.post(recordingMeterClock)
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
        mainHandler.removeCallbacks(recordingMeterClock)
        inputMeter.progress = 0
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
        diagnosticsCapture.record("recording_finalize", if (nativeStatus.startsWith("Saved recording:")) "success" else "failure",
            mapOf("native_status" to nativeStatus.replace(filesDir.absolutePath, "<app-private>").take(1200), "staging_name" to (stagedFile?.name ?: "missing"),
                "staging_exists" to (stagedFile?.exists() == true), "staging_bytes" to (stagedFile?.length() ?: -1L)))
        if (!nativeStatus.startsWith("Saved recording:")) {
            statusText.text = interruptionNotice + "RECOVERABLE / INCOMPLETE TAKE — finalization did not succeed.\n\n" + nativeStatus +
                "\n\nThe recoverable partial, if present, was retained in app-private storage. Open “Check for interrupted recordings” to inspect it."
            return
        }
        if (stagedFile == null || !isCompletePcmWav(stagedFile)) {
            diagnosticsCapture.record("recording_validation", "failure", mapOf(
                "staging_name" to (stagedFile?.name ?: "missing"),
                "staging_exists" to (stagedFile?.exists() == true),
                "staging_bytes" to (stagedFile?.length() ?: -1L)))
            statusText.text = interruptionNotice +
                "UNVERIFIED TAKE — WAV validation failed. The staged file was retained in app-private storage and was not exported."
            return
        }

        val takeDetails = recordingTakeDetails(stagedFile)
        statusText.text = try {
            val publicUri = publishRecording(stagedFile)
            if (publicUri != null) {
                diagnosticsCapture.record("recording_export", "success", mapOf("staging_name" to stagedFile.name, "source_bytes" to stagedFile.length()))
                if (!stagedFile.delete()) {
                    interruptionNotice + "FINALIZED AND VERIFIED — recording exported successfully, but the private staging copy could not be removed."
                } else {
                    interruptionNotice + "FINALIZED AND VERIFIED — saved WAV in Music/Audio Studio/Recordings."
                }
            } else {
                interruptionNotice + "FINALIZED AND VERIFIED — automatic public export is unavailable on this Android version. The verified WAV remains in app-private storage."
            }
        } catch (exception: Exception) {
            diagnosticsCapture.record("recording_export", "failure", mapOf("staging_name" to stagedFile.name, "source_bytes" to stagedFile.length()), exception)
            interruptionNotice + "FINALIZED AND VERIFIED — export failed: ${exception.message ?: "unknown error"}. The verified WAV remains in app-private storage."
        }.let { message ->
            if (takeDetails != null) message + "\n\n" + takeDetails else message
        }
    }

    private fun recordingTakeDetails(file: File): String? {
        return try {
            val metadata = nativeGetWavMetadata(file.absolutePath)
            if (metadata.size < 3 || metadata[0] <= 0L || metadata[1] <= 0L || metadata[2] <= 0L) {
                return null
            }
            val seconds = metadata[0] / metadata[1]
            val minutes = seconds / 60L
            val remainder = seconds % 60L
            val channels = if (metadata[2] == 1L) "Mono" else "Stereo"
            val megabytes = file.length().toDouble() / (1024.0 * 1024.0)
            "Take details: %02d:%02d · %d kHz · %s · %.2f MB".format(
                minutes, remainder, metadata[1] / 1000L, channels, megabytes
            )
        } catch (_: Exception) {
            null // Details are optional; never turn a successfully saved take into a failure.
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


    private fun chooseWavWaveform() {
        if (recording || processing || wavPickerActive || startAfterPermission) {
            waveformStatus.text = "Finish the current operation before loading a waveform."
            return
        }
        val picker = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "audio/*"
        }
        wavPickerActive = true
        waveformButton.isEnabled = false
        processButton.isEnabled = false
        startButton.isEnabled = false
        try { startActivityForResult(picker, REQUEST_WAVEFORM) }
        catch (exception: Exception) {
            wavPickerActive = false
            waveformButton.isEnabled = true
            processButton.isEnabled = true
            startButton.isEnabled = true
            waveformStatus.text = "Could not open file picker: ${exception.message ?: "unknown error"}"
        }
    }

    private fun analyzeSelectedWaveform(uri: Uri) {
        diagnosticsCapture.record("waveform_load", "started", mapOf("source_uri_authority" to (uri.authority ?: "unknown")))
        waveformStatus.text = "Analyzing WAV in bounded memory…"
        Thread {
            var staged: File? = null
            try {
                val file = File(filesDir, uniqueWavName("waveform-source"))
                staged = file
                contentResolver.openInputStream(uri)?.use { input ->
                    file.outputStream().use { output ->
                        val buffer = ByteArray(64 * 1024)
                        while (true) { val count = input.read(buffer); if (count < 0) break; output.write(buffer, 0, count) }
                    }
                } ?: throw IOException("Could not open the selected file.")
                val peaks = nativeAnalyzeWavWaveform(file.absolutePath, 240)
                if (peaks.isEmpty()) {
                    val detail = nativeWavAnalysisError(file.absolutePath)
                    throw IOException(
                        if (detail.startsWith("ERROR:")) detail.removePrefix("ERROR:").trim()
                        else "Could not analyze the selected WAV. Choose a valid mono/stereo PCM16 WAV."
                    )
                }
                if (peaks.size != 960) throw IOException("Waveform analysis returned an incomplete result. Try another PCM16 WAV.")
                val metadata = nativeGetWavMetadata(file.absolutePath)
                if (metadata.size < 3 || metadata[0] <= 0L || metadata[1] <= 0L) throw IOException("Could not read WAV duration metadata.")
                val previousSource = waveformSourceFile
                waveformSourceFile = file
                waveformTotalFrames = metadata[0]
                waveformSampleRate = metadata[1]
                diagnosticsCapture.record("waveform_load", "success", mapOf(
                    "staged_name" to file.name, "staged_bytes" to file.length(),
                    "frames" to metadata[0], "sample_rate_hz" to metadata[1], "channels" to metadata[2]))
                if (previousSource != null && previousSource != file) previousSource.delete()
                mainHandler.post {
                    if (!isFinishing && !isDestroyed) {
                        waveformView.setPeaks(peaks)
                        exactTrimStartFrame = null
                        exactTrimEndFrame = null
                        trimStartSeekBar.progress = 0
                        trimEndSeekBar.progress = 1000
                        waveformView.setSelection(0f, 1f)
                        updateTrimSelectionSummary()
                        trimWaveformButton.isEnabled = true
                        previewSelectionButton.isEnabled = true
                        waveformStatus.text = "Waveform loaded · select a range below. Original file unchanged."
                    }
                }
            } catch (exception: Exception) {
                diagnosticsCapture.record("waveform_load", "failure", mapOf("staged_name" to (staged?.name ?: "missing"),
                    "staged_bytes" to (staged?.length() ?: -1L)), exception)
                mainHandler.post { if (!isFinishing && !isDestroyed) waveformStatus.text = exception.message ?: "Waveform analysis failed." }
            } finally {
                if (staged != waveformSourceFile) staged?.delete()
            }
        }.start()
    }


    private fun previewTrimSelection() {
        if (recording || processing || wavPickerActive || startAfterPermission) {
            waveformStatus.text = "Wait until the current operation finishes before previewing."
            return
        }
        val source = waveformSourceFile
        if (source == null || !source.isFile || waveformTotalFrames <= 0L || waveformSampleRate <= 0L) {
            waveformStatus.text = "Load a WAV waveform before previewing a selection."
            return
        }
        val (startFrame, endFrame) = selectedTrimFrames()
        if (startFrame >= endFrame) {
            waveformStatus.text = "Choose a non-empty selection to preview."
            return
        }
        val startMs = (startFrame * 1000L / waveformSampleRate).coerceAtMost(Int.MAX_VALUE.toLong()).toInt()
        val endMs = (endFrame * 1000L / waveformSampleRate).coerceAtMost(Int.MAX_VALUE.toLong()).toInt()
        if (endMs <= startMs) {
            waveformStatus.text = "The selected range is too short to preview."
            return
        }
        stopComparisonPlayback()
        try {
            val player = MediaPlayer()
            playback = player
            trimPreviewEndMs = endMs
            player.setDataSource(source.absolutePath)
            player.setOnPreparedListener { prepared ->
                if (playback === prepared && !isFinishing && !isDestroyed) {
                    val safeStart = startMs.coerceAtMost((prepared.duration - 1).coerceAtLeast(0))
                    val safeEnd = endMs.coerceAtMost(prepared.duration)
                    if (safeEnd <= safeStart) {
                        stopComparisonPlayback()
                        waveformStatus.text = "Could not preview this range; choose a longer selection."
                        return@setOnPreparedListener
                    }
                    trimPreviewEndMs = safeEnd
                    prepared.seekTo(safeStart)
                    prepared.start()
                    stopPlaybackButton.isEnabled = true
                    statusText.text = "Previewing selected range…"
                    mainHandler.postDelayed(trimPreviewClock, 25L)
                } else {
                    prepared.release()
                }
            }
            player.setOnCompletionListener { completed ->
                if (playback === completed) {
                    mainHandler.removeCallbacks(trimPreviewClock)
                    completed.release()
                    playback = null
                    trimPreviewEndMs = -1
                    stopPlaybackButton.isEnabled = false
                    statusText.text = "Selection preview finished. Adjust the range or export the trim."
                }
            }
            player.setOnErrorListener { failed, _, _ ->
                if (playback === failed) {
                    mainHandler.removeCallbacks(trimPreviewClock)
                    failed.release()
                    playback = null
                    trimPreviewEndMs = -1
                }
                stopPlaybackButton.isEnabled = false
                waveformStatus.text = "Preview playback failed. Try another PCM16 WAV."
                true
            }
            statusText.text = "Preparing selection preview…"
            player.prepareAsync()
        } catch (exception: Exception) {
            stopComparisonPlayback()
            waveformStatus.text = "Could not preview selection: ${exception.message ?: "unknown error"}"
        }
    }

    private fun selectedTrimFrames(): Pair<Long, Long> {
        val start = exactTrimStartFrame ?: (waveformTotalFrames * trimStartSeekBar.progress / 1000L)
        val end = exactTrimEndFrame ?: (waveformTotalFrames * trimEndSeekBar.progress / 1000L)
        return start.coerceIn(0L, waveformTotalFrames) to end.coerceIn(0L, waveformTotalFrames)
    }

    private fun parseTrimTime(value: String): Long? {
        val parts = value.trim().split(':')
        if (parts.size != 2 || parts[0].isEmpty()) return null
        val minutes = parts[0].toLongOrNull() ?: return null
        if (minutes < 0L) return null
        val secondParts = parts[1].split('.')
        if (secondParts.size !in 1..2) return null
        val seconds = secondParts[0].toIntOrNull() ?: return null
        if (seconds !in 0..59) return null
        var millis = 0
        if (secondParts.size == 2) {
            val fraction = secondParts[1]
            if (fraction.length !in 1..3 || !fraction.all { it in '0'..'9' }) return null
            millis = fraction.toIntOrNull() ?: return null
            millis *= when (fraction.length) { 1 -> 100; 2 -> 10; else -> 1 }
        }
        val totalMillis = try {
            Math.addExact(Math.addExact(Math.multiplyExact(minutes, 60_000L), seconds * 1000L), millis.toLong())
        } catch (_: ArithmeticException) {
            return null
        }
        return totalMillis
    }

    private fun framesFromMillis(milliseconds: Long): Long? {
        if (waveformSampleRate <= 0L || milliseconds < 0L) return null
        val frames = try {
            Math.addExact(Math.multiplyExact(milliseconds, waveformSampleRate), 500L) / 1000L
        } catch (_: ArithmeticException) {
            return null
        }
        return frames.takeIf { it <= waveformTotalFrames }
    }

    private fun applyTrimTimecodes() {
        if (waveformTotalFrames <= 0L || waveformSampleRate <= 0L) {
            waveformStatus.text = "Load a WAV waveform before setting exact in/out points."
            return
        }
        val startMs = parseTrimTime(trimStartTimeInput.text.toString())
        val endMs = parseTrimTime(trimEndTimeInput.text.toString())
        if (startMs == null || endMs == null) {
            waveformStatus.text = "Use minutes:seconds.milliseconds, for example 02:14.350."
            return
        }
        val startFrame = framesFromMillis(startMs)
        val endFrame = framesFromMillis(endMs)
        if (startFrame == null || endFrame == null || endFrame <= startFrame) {
            waveformStatus.text = "Choose an in point before the out point, both within the loaded WAV."
            return
        }

        applyingTrimTimecodes = true
        try {
            trimStartSeekBar.progress = ((startFrame.toDouble() / waveformTotalFrames) * 1000.0)
                .toInt().coerceIn(0, 1000)
            trimEndSeekBar.progress = ((endFrame.toDouble() / waveformTotalFrames) * 1000.0)
                .toInt().coerceIn(0, 1000)
        } finally {
            applyingTrimTimecodes = false
        }
        exactTrimStartFrame = startFrame
        exactTrimEndFrame = endFrame
        waveformView.setSelection(
            startFrame.toFloat() / waveformTotalFrames,
            endFrame.toFloat() / waveformTotalFrames
        )
        updateTrimSelectionSummary()
        waveformStatus.text = "Exact in/out points applied. Preview and export use these audio-frame positions; moving a slider adjusts that endpoint."
    }

    private fun updateTrimSelectionSummary() {
        if (waveformTotalFrames <= 0L || waveformSampleRate <= 0L) {
            trimSelectionSummary.text = "Selected duration: load a WAV"
            return
        }
        val (startFrame, endFrame) = selectedTrimFrames()
        val startSeconds = startFrame.toDouble() / waveformSampleRate
        val endSeconds = endFrame.toDouble() / waveformSampleRate
        val selectedSeconds = (endFrame - startFrame).coerceAtLeast(0L).toDouble() / waveformSampleRate
        trimStartTimeInput.setText(formatTrimTime(startSeconds))
        trimEndTimeInput.setText(formatTrimTime(endSeconds))
        trimSelectionSummary.text = "Selection: ${formatTrimTime(startSeconds)} – ${formatTrimTime(endSeconds)} · ${"%.2f".format(java.util.Locale.ROOT, selectedSeconds)} s"
    }

    private fun formatTrimTime(seconds: Double): String {
        val totalMillis = (seconds * 1000.0).toLong().coerceAtLeast(0L)
        val minutes = totalMillis / 60000L
        val remainingSeconds = (totalMillis % 60000L) / 1000L
        val millis = totalMillis % 1000L
        return "%02d:%02d.%03d".format(java.util.Locale.ROOT, minutes, remainingSeconds, millis)
    }

    private fun trimRangeListener(onProgress: (Int) -> Unit) =
        object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                onProgress(progress)
            }
            override fun onStartTrackingTouch(seekBar: SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: SeekBar?) {}
        }

    private fun chooseTrimOutput() {
        if (recording || processing || wavPickerActive) {
            waveformStatus.text = "Finish the current operation before trimming."
            return
        }
        val (startFrame, endFrame) = selectedTrimFrames()
        if (waveformSourceFile == null || startFrame >= endFrame) {
            waveformStatus.text = "Choose a loaded waveform and a non-empty trim range."
            return
        }
        val intent = Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "audio/wav"
            putExtra(Intent.EXTRA_TITLE, uniqueWavName("AudioStudio-trimmed"))
        }
        wavPickerActive = true
        trimWaveformButton.isEnabled = false
        try {
            startActivityForResult(intent, REQUEST_TRIM_OUTPUT)
        } catch (exception: Exception) {
            wavPickerActive = false
            trimWaveformButton.isEnabled = waveformSourceFile != null
            waveformStatus.text = "Could not choose trim destination: ${exception.message ?: "unknown error"}"
        }
    }

    private fun trimSelectedWaveform(destination: Uri) {
        val source = waveformSourceFile ?: run {
            waveformStatus.text = "Load a waveform before trimming."
            trimWaveformButton.isEnabled = false
            return
        }
        val (startFrame, endFrame) = selectedTrimFrames()
        if (startFrame >= endFrame) {
            waveformStatus.text = "Trim start must be before trim end."
            trimWaveformButton.isEnabled = true
            return
        }
        waveformStatus.text = "Trimming selected range…"
        Thread {
            val stagedOutput = File(filesDir, uniqueWavName("waveform-trimmed"))
            try {
                val result = nativeTrimWav(source.absolutePath, stagedOutput.absolutePath, startFrame, endFrame)
                if (result != "TRIMMED") {
                    diagnosticsCapture.record("trim_render", "failure", mapOf(
                        "source_name" to source.name, "source_bytes" to source.length(),
                        "output_name" to stagedOutput.name, "native_status" to result,
                        "start_frame" to startFrame, "end_frame" to endFrame), IOException(result))
                    throw IOException(result)
                }
                diagnosticsCapture.record("trim_render", "success", mapOf(
                    "source_name" to source.name, "source_bytes" to source.length(),
                    "output_name" to stagedOutput.name, "output_bytes" to stagedOutput.length(),
                    "start_frame" to startFrame, "end_frame" to endFrame))
                val metadata = nativeGetWavMetadata(stagedOutput.absolutePath)
                if (metadata.size < 3 || metadata[0] <= 0L || metadata[1] <= 0L ||
                    metadata[2] !in 1L..2L || stagedOutput.length() <= WAV_HEADER_BYTES) {
                    throw IOException("Trim output failed WAV verification and was not exported. Original recording preserved.")
                }
                var copiedBytes = 0L
                contentResolver.openOutputStream(destination, "w")?.use { output ->
                    FileInputStream(stagedOutput).use { input ->
                        val buffer = ByteArray(64 * 1024)
                        while (true) {
                            val count = input.read(buffer)
                            if (count < 0) break
                            output.write(buffer, 0, count)
                            copiedBytes += count
                        }
                    }
                } ?: throw IOException("Could not open the chosen output file.")
                if (copiedBytes != stagedOutput.length() || copiedBytes <= WAV_HEADER_BYTES) {
                    throw IOException("Trim export was incomplete ($copiedBytes of ${stagedOutput.length()} bytes written). The original recording is preserved.")
                }
                val exportedBytes = verifyExportedWav(destination, stagedOutput.length())
                diagnosticsCapture.record("trim_export", "success", mapOf(
                    "source_name" to source.name,
                    "output_bytes" to stagedOutput.length(),
                    "destination_bytes_verified" to exportedBytes,
                    "selected_frames" to (endFrame - startFrame)))
                val seconds = metadata[0] / metadata[1]
                val minutes = seconds / 60L
                val remainder = seconds % 60L
                val channelLabel = if (metadata[2] == 1L) "Mono" else "Stereo"
                mainHandler.post {
                    if (!isFinishing && !isDestroyed)
                        waveformStatus.text = "Trimmed WAV verified and saved · %02d:%02d · %d kHz · %s. Original recording preserved.".format(
                            java.util.Locale.ROOT, minutes, remainder, metadata[1] / 1000L, channelLabel
                        )
                }
            } catch (exception: Exception) {
                diagnosticsCapture.record("trim_export", "failure", mapOf(
                    "source_name" to (source.name),
                    "staged_output_exists" to stagedOutput.isFile,
                    "staged_output_bytes" to stagedOutput.length(),
                    "selected_frames" to (endFrame - startFrame)), exception)
                mainHandler.post {
                    if (!isFinishing && !isDestroyed)
                        waveformStatus.text = (exception.message ?: "Trim failed. Original file preserved.") +
                            " The source was not modified; inspect the chosen destination before reusing it."
                }
            } finally {
                stagedOutput.delete()
                mainHandler.post {
                    if (!isFinishing && !isDestroyed)
                        trimWaveformButton.isEnabled = waveformSourceFile != null
                }
            }
        }.start()
    }

    private fun chooseWavToProcess() {
        if (recording || processing || wavPickerActive || startAfterPermission) {
            processingStatus.text = "Stop the current operation before processing a WAV."
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
            processingStatus.text = "Could not open the document picker: ${exception.message ?: "unknown error"}"
        }
    }

    @Deprecated("Uses the platform document picker result API for broad Android compatibility")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == REQUEST_DIAGNOSTICS_EXPORT) {
            if (resultCode == RESULT_OK && data?.data != null) exportDiagnostics(data.data!!)
            else diagnosticsCapture.record("diagnostics_export", "cancelled")
            return
        }
        if (requestCode == REQUEST_TRIM_OUTPUT) {
            wavPickerActive = false
            diagnosticsCapture.record("trim_destination_picker", if (resultCode == RESULT_OK && data?.data != null) "selected" else "cancelled",
                mapOf("result_code" to resultCode, "uri_authority" to (data?.data?.authority ?: "")))
            if (resultCode == RESULT_OK && data?.data != null) trimSelectedWaveform(data.data!!)
            else {
                waveformStatus.text = "Trim cancelled. No files were changed."
                trimWaveformButton.isEnabled = waveformSourceFile != null
            }
            return
        }
        if (requestCode == REQUEST_WAVEFORM) {
            wavPickerActive = false
            diagnosticsCapture.record("waveform_picker", if (resultCode == RESULT_OK && data?.data != null) "selected" else "cancelled",
                mapOf("result_code" to resultCode, "uri_authority" to (data?.data?.authority ?: "")))
            if (resultCode == RESULT_OK && data?.data != null) analyzeSelectedWaveform(data.data!!)
            else waveformStatus.text = "Waveform selection cancelled."
            processButton.isEnabled = !recording && !processing
            startButton.isEnabled = !recording && !processing
            waveformButton.isEnabled = !recording && !processing
            return
        }
        if (requestCode != REQUEST_PROCESS_WAV) return
        diagnosticsCapture.record("processing_picker", if (resultCode == RESULT_OK && data?.data != null) "selected" else "cancelled",
            mapOf("result_code" to resultCode, "uri_authority" to (data?.data?.authority ?: "")))
        wavPickerActive = false
        if (resultCode != RESULT_OK) {
            processingStatus.text = "WAV selection cancelled. Nothing was processed."
            processButton.isEnabled = true
            startButton.isEnabled = true
            return
        }
        val uri = data?.data ?: run {
            processingStatus.text = "No WAV file was selected."
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
        val gainDb = (dspGainSeekBar.progress - 24).toFloat()
        val highPassHz = if (highPassSeekBar.progress == 0) 0.0F else
            (20 + (highPassSeekBar.progress - 1) * 10).coerceAtMost(500).toFloat()
        processingStatus.text = "Preparing WAV for noise reduction…"
        Thread {
            val stagingDirectory = File(filesDir, "processing-staging")
            var input: File? = null
            var output: File? = null
            var keepComparisonFiles = false
            try {
                if (!stagingDirectory.exists() && !stagingDirectory.mkdirs()) {
                    throw IOException("Could not create processing staging folder.")
                }
                val inputFile = File(stagingDirectory, uniqueWavName("source"))
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
                val outputFile = File(stagingDirectory, uniqueWavName("processed"))
                output = outputFile
                if (cancelRequested) throw IOException("Processing cancelled by user.")
                val callback = object : NoiseGateProgressCallback {
                    override fun onProgress(processedFrames: Long, totalFrames: Long) {
                        val percent = if (totalFrames <= 0L) 0 else
                            ((processedFrames.coerceAtLeast(0L).toDouble() / totalFrames.toDouble()) * 100.0).toInt().coerceIn(0, 100)
                        mainHandler.post {
                            if (!isFinishing && !isDestroyed && processing) {
                                processingProgress.progress = percent
                                processingStatus.text = "Reducing background noise… $percent%"
                            }
                        }
                    }
                }
                val nativeStatus = nativeProcessNoiseGate(
                    inputFile.absolutePath,
                    outputFile.absolutePath,
                    thresholdDbfs,
                    attenuationDb,
                    gainDb,
                    highPassHz,
                    callback
                )
                if (!nativeStatus.startsWith("Processed WAV:")) {
                    diagnosticsCapture.record("noise_gate_process", "failure", mapOf(
                        "source_name" to inputFile.name, "source_bytes" to inputFile.length(),
                        "native_status" to nativeStatus.take(1200)), IOException(nativeStatus))
                    throw IOException(nativeStatus.removePrefix("ERROR:").trim())
                }
                if (!isCompletePcmWav(outputFile)) {
                    throw IOException("The processed file did not pass WAV integrity checks.")
                }
                if (cancelRequested) throw IOException("Processing cancelled by user.")
                val publicUri = publishRecording(outputFile, "Processed") { cancelRequested }
                    ?: throw IOException("Public export requires Android 10 or later. The processed file remains in app-private storage.")
                diagnosticsCapture.record("noise_gate_export", "success", mapOf(
                    "output_name" to outputFile.name, "source_bytes" to inputFile.length(),
                    "output_bytes" to outputFile.length()))
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
                        processingStatus.text = "Noise-reduced copy saved as " + outputName + ". Use Play original and Play noise-reduced copy to compare them. Your source is unchanged."
                    }
                }
            } catch (exception: Exception) {
                diagnosticsCapture.record("noise_gate_process", "failure", mapOf(
                    "input_name" to (input?.name ?: "missing"), "input_bytes" to (input?.length() ?: -1L),
                    "output_name" to (output?.name ?: "missing"), "output_bytes" to (output?.length() ?: -1L),
                    "cancel_requested" to cancelRequested), exception)
                mainHandler.post {
                    if (!isFinishing && !isDestroyed) {
                        val message = exception.message ?: "unknown error"
                        processingStatus.text = if (cancelRequested || message.contains("cancelled", ignoreCase = true)) {
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
            processingStatus.text = "Wait until the current operation finishes before playback."
            return
        }
        if (file == null || !file.isFile || file.length() <= WAV_HEADER_BYTES) {
            processingStatus.text = "No comparison file is available. Process a WAV first."
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
                    processingStatus.text = "Playing " + label + " recording. You can switch to the other version at any time."
                } else {
                    prepared.release()
                }
            }
            player.setOnCompletionListener { completed ->
                if (playback === completed) {
                    completed.release()
                    playback = null
                    stopPlaybackButton.isEnabled = false
                    processingStatus.text = "Playback finished. Compare the other version or process another WAV."
                }
            }
            player.setOnErrorListener { failed, _, _ ->
                if (playback === failed) {
                    failed.release()
                    playback = null
                }
                stopPlaybackButton.isEnabled = false
                processingStatus.text = "Playback failed for the " + label + " file. Try another PCM16 WAV."
                true
            }
            stopPlaybackButton.isEnabled = true
            processingStatus.text = "Preparing " + label + " playback…"
            player.prepareAsync()
        } catch (exception: Exception) {
            playback?.release()
            playback = null
            stopPlaybackButton.isEnabled = false
            processingStatus.text = "Could not play " + label + " recording: " + (exception.message ?: "unknown error")
        }
    }

    private fun stopComparisonPlayback() {
        mainHandler.removeCallbacks(trimPreviewClock)
        trimPreviewEndMs = -1
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
        diagnosticsCapture.record("media_store_export", "started", mapOf(
            "source_name" to source.name, "source_bytes" to source.length(), "folder" to folder))
        val destination = contentResolver.insert(collection, values)
            ?: throw IOException("Android could not create the public WAV entry")

        var copiedBytes = 0L
        try {
            if (!source.isFile) throw IOException("The source WAV is no longer available.")
            val expectedBytes = source.length()
            if (expectedBytes <= 0L) throw IOException("The source WAV is empty.")
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
                        copiedBytes += count.toLong()
                    }
                }
            }

            // Do not make a public MediaStore item visible until every source byte
            // has been copied. A partial copy must be deleted, not published as a WAV.
            if (copiedBytes != expectedBytes || !source.isFile || source.length() != expectedBytes) {
                throw IOException(
                    "WAV export was incomplete ($copiedBytes of $expectedBytes bytes copied)."
                )
            }
            if (isCancelled?.invoke() == true) {
                throw IOException("Export cancelled by user.")
            }
            val verifiedBytes = verifyExportedWav(destination, expectedBytes)
            if (verifiedBytes != expectedBytes) {
                throw IOException("WAV export read-back size did not match the source ($verifiedBytes of $expectedBytes bytes).")
            }

            val completedValues = ContentValues().apply {
                put(MediaStore.Audio.Media.IS_PENDING, 0)
            }
            if (contentResolver.update(destination, completedValues, null, null) <= 0) {
                throw IOException("Android could not finalize the exported WAV")
            }
            diagnosticsCapture.record("media_store_export", "success", mapOf(
                "source_name" to source.name, "source_bytes" to expectedBytes, "copied_bytes" to copiedBytes,
                "verified_bytes" to verifiedBytes, "folder" to folder))
            return destination
        } catch (exception: Exception) {
            diagnosticsCapture.record("media_store_export", "failure", mapOf(
                "source_name" to source.name, "source_bytes" to source.length(), "copied_bytes" to copiedBytes,
                "folder" to folder), exception)
            contentResolver.delete(destination, null, null)
            throw exception
        }
    }

    /** Read the destination back after close; catches providers that return a successful stream but persist 0 bytes. */
    @Throws(IOException::class)
    private fun verifyExportedWav(uri: Uri, expectedBytes: Long): Long {
        var count = 0L
        val header = ByteArray(12)
        var headerCount = 0
        contentResolver.openInputStream(uri)?.use { input ->
            val buffer = ByteArray(64 * 1024)
            while (true) {
                val read = input.read(buffer)
                if (read < 0) break
                if (read == 0) continue
                if (headerCount < header.size) {
                    val copy = minOf(read, header.size - headerCount)
                    System.arraycopy(buffer, 0, header, headerCount, copy)
                    headerCount += copy
                }
                count += read.toLong()
            }
        } ?: throw IOException("Could not reopen the exported WAV for verification.")
        if (count != expectedBytes) {
            throw IOException("Export read-back size mismatch: expected $expectedBytes bytes, found $count.")
        }
        if (count <= WAV_HEADER_BYTES || headerCount < 12 ||
            String(header, 0, 4, Charsets.US_ASCII) != "RIFF" ||
            String(header, 8, 4, Charsets.US_ASCII) != "WAVE") {
            throw IOException("Exported destination is empty or does not contain a RIFF/WAVE header.")
        }
        return count
    }

    private fun chooseDiagnosticsExport() {
        val intent = Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "text/plain"
            putExtra(Intent.EXTRA_TITLE, "audio-studio-diagnostics.txt")
        }
        try {
            startActivityForResult(intent, REQUEST_DIAGNOSTICS_EXPORT)
            diagnosticsCapture.record("diagnostics_export", "picker_opened")
        } catch (exception: Exception) {
            diagnosticsCapture.record("diagnostics_export", "picker_failed", error = exception)
            libraryStatusMessage("Could not open diagnostics export: ${exception.message ?: "unknown error"}")
        }
    }

    private fun libraryStatusMessage(message: String) {
        // Find the current library status without rebuilding the screen.
        diagnosticsCapture.record("diagnostics_ui_message", "info", mapOf("message" to message))
        statusText.text = message
    }

    private fun exportDiagnostics(destination: Uri) {
        Thread {
            try {
                val report = diagnosticsCapture.reportBytes()
                contentResolver.openOutputStream(destination, "w")?.use { it.write(report) }
                    ?: throw IOException("Could not open the chosen diagnostics destination.")
                val exported = contentResolver.openInputStream(destination)?.use { input ->
                    var count = 0L
                    val buffer = ByteArray(8192)
                    while (true) {
                        val n = input.read(buffer)
                        if (n < 0) break
                        count += n
                    }
                    count
                } ?: throw IOException("Could not verify the exported diagnostics report.")
                if (exported != report.size.toLong()) throw IOException("Diagnostics report verification failed.")
                diagnosticsCapture.record("diagnostics_export", "success", mapOf("report_bytes" to exported))
                mainHandler.post { if (!isFinishing && !isDestroyed) statusText.text = "Diagnostics report exported and verified. It contains no audio samples." }
            } catch (exception: Exception) {
                diagnosticsCapture.record("diagnostics_export", "failure", error = exception)
                mainHandler.post { if (!isFinishing && !isDestroyed) statusText.text = "Diagnostics export failed: ${exception.message ?: "unknown error"}" }
            }
        }.start()
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
        diagnosticsCapture.record("activity_lifecycle", "on_stop", mapOf("recording" to recording, "processing" to processing))
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
        mainHandler.removeCallbacks(recordingMeterClock)
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
        private const val REQUEST_WAVEFORM = 1003
        private const val REQUEST_TRIM_OUTPUT = 1004
        private const val REQUEST_DIAGNOSTICS_EXPORT = 1005
        private const val WAV_HEADER_BYTES = 44L
        private const val STALE_PROCESSING_FILE_AGE_MS = 24L * 60L * 60L * 1000L
    }
}
