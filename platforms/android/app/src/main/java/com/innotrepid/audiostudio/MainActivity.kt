package com.innotrepid.audiostudio

import android.Manifest
import android.app.Activity
import android.app.AlertDialog
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
import android.widget.ScrollView
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
    private external fun nativeRecoverPartialWav(partialPath: String, outputPath: String): String
    private external fun nativeAnalyzeWavWaveform(inputPath: String, bucketCount: Int): FloatArray
    private external fun nativeGetWavMetadata(inputPath: String): LongArray
    private external fun nativeTrimWav(inputPath: String, outputPath: String, startFraction: Long, endFraction: Long): String
    private external fun nativeStopRecording(): String
    private external fun nativeRecordingMeter(): FloatArray
    private external fun nativeProcessNoiseGate(inputPath: String, outputPath: String, thresholdDbfs: Float, attenuationDb: Float, gainDb: Float, highPassHz: Float, callback: NoiseGateProgressCallback): String
    private external fun nativeCancelNoiseGateProcessing()
    private external fun nativePrepareNoiseGateProcessing()

    private lateinit var statusText: TextView
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
                trimEndLabel.text = "Trim end: ${it / 10.0f}%"
                waveformView.setSelection(trimStartSeekBar.progress / 1000f, it / 1000f)
                updateTrimSelectionSummary()
            })
        }
        trimSelectionSummary = TextView(this).apply {
            text = "Selected duration: load a WAV"
            textSize = 13f
            setTextColor(Color.rgb(190, 198, 214))
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
                    if (fromUser) gatePreferences.edit().putInt("dsp_gain_progress", progress).apply()
                }
                override fun onStartTrackingTouch(seekBar: SeekBar?) = Unit
                override fun onStopTrackingTouch(seekBar: SeekBar?) = Unit
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
                    if (fromUser) gatePreferences.edit().putInt("high_pass_progress", progress).apply()
                }
                override fun onStartTrackingTouch(seekBar: SeekBar?) = Unit
                override fun onStopTrackingTouch(seekBar: SeekBar?) = Unit
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
        editSection.addView(trimSelectionSummary, matchWidth())
        editSection.addView(previewSelectionButton, matchWidth())
        editSection.addView(trimWaveformButton, matchWidth())
        editSection.addView(waveformStatus, matchWidth())

        val processingSection = makeSection("Clean up audio", "Choose a preset first. Advanced controls are available below; processing creates a separate copy.")
        processingSection.addView(processButton, matchWidth())
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

        fun sectionLayoutParams() = LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
        ).apply { bottomMargin = 18 }
        root.addView(recordSection, sectionLayoutParams())
        root.addView(editSection, sectionLayoutParams())
        root.addView(processingSection, sectionLayoutParams())
        // Keep the current workflow intact while ensuring every control is reachable
        // on smaller phones as the navigation shell is introduced in later UI phases.
        val screenScroll = ScrollView(this).apply {
            isFillViewport = true
            clipToPadding = false
            setBackgroundColor(Color.rgb(16, 18, 24))
            addView(root, ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            ))
        }
        setContentView(screenScroll)
        root.post { showInterruptedRecordings(showWhenNone = false) }
    }

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
                val outputName = partial.name.replace(Regex("\\.partial-[0-9]+$"), "")
                val output = File(partial.parentFile, outputName)
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


    private fun chooseWavWaveform() {
        if (recording || processing || wavPickerActive || startAfterPermission) {
            statusText.text = "Finish the current operation before loading a waveform."
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
        waveformStatus.text = "Analyzing WAV in bounded memory…"
        Thread {
            var staged: File? = null
            try {
                val file = File(filesDir, "waveform-source-${System.currentTimeMillis()}.wav")
                staged = file
                contentResolver.openInputStream(uri)?.use { input ->
                    file.outputStream().use { output ->
                        val buffer = ByteArray(64 * 1024)
                        while (true) { val count = input.read(buffer); if (count < 0) break; output.write(buffer, 0, count) }
                    }
                } ?: throw IOException("Could not open the selected file.")
                val peaks = nativeAnalyzeWavWaveform(file.absolutePath, 240)
                if (peaks.size != 960) throw IOException("Could not analyze this file. Choose a valid PCM16 WAV.")
                val metadata = nativeGetWavMetadata(file.absolutePath)
                if (metadata.size < 3 || metadata[0] <= 0L || metadata[1] <= 0L) throw IOException("Could not read WAV duration metadata.")
                val previousSource = waveformSourceFile
                waveformSourceFile = file
                waveformTotalFrames = metadata[0]
                waveformSampleRate = metadata[1]
                if (previousSource != null && previousSource != file) previousSource.delete()
                mainHandler.post {
                    if (!isFinishing && !isDestroyed) {
                        waveformView.setPeaks(peaks)
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
        val startFrame = waveformTotalFrames * trimStartSeekBar.progress / 1000L
        val endFrame = waveformTotalFrames * trimEndSeekBar.progress / 1000L
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

    private fun updateTrimSelectionSummary() {
        if (waveformTotalFrames <= 0L || waveformSampleRate <= 0L) {
            trimSelectionSummary.text = "Selected duration: load a WAV"
            return
        }
        val startFrame = waveformTotalFrames * trimStartSeekBar.progress / 1000L
        val endFrame = waveformTotalFrames * trimEndSeekBar.progress / 1000L
        val startSeconds = startFrame.toDouble() / waveformSampleRate
        val endSeconds = endFrame.toDouble() / waveformSampleRate
        val selectedSeconds = (endFrame - startFrame).coerceAtLeast(0L).toDouble() / waveformSampleRate
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
        if (waveformSourceFile == null || trimStartSeekBar.progress >= trimEndSeekBar.progress) {
            waveformStatus.text = "Choose a loaded waveform and a non-empty trim range."
            return
        }
        val intent = Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "audio/wav"
            putExtra(Intent.EXTRA_TITLE, "AudioStudio-trimmed.wav")
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
        val startFraction = trimStartSeekBar.progress.toLong()
        val endFraction = trimEndSeekBar.progress.toLong()
        if (startFraction >= endFraction) {
            waveformStatus.text = "Trim start must be before trim end."
            trimWaveformButton.isEnabled = true
            return
        }
        waveformStatus.text = "Trimming selected range…"
        Thread {
            val stagedOutput = File(filesDir, "waveform-trimmed-${System.currentTimeMillis()}.wav")
            try {
                val result = nativeTrimWav(source.absolutePath, stagedOutput.absolutePath, startFraction, endFraction)
                if (result != "TRIMMED") throw IOException(result)
                contentResolver.openOutputStream(destination, "w")?.use { output ->
                    FileInputStream(stagedOutput).use { input ->
                        val buffer = ByteArray(64 * 1024)
                        while (true) {
                            val count = input.read(buffer)
                            if (count < 0) break
                            output.write(buffer, 0, count)
                        }
                    }
                } ?: throw IOException("Could not open the chosen output file.")
                mainHandler.post {
                    if (!isFinishing && !isDestroyed)
                        waveformStatus.text = "Trimmed WAV saved. Original recording preserved."
                }
            } catch (exception: Exception) {
                mainHandler.post {
                    if (!isFinishing && !isDestroyed)
                        waveformStatus.text = exception.message ?: "Trim failed. Original file preserved."
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
        if (requestCode == REQUEST_TRIM_OUTPUT) {
            wavPickerActive = false
            if (resultCode == RESULT_OK && data?.data != null) trimSelectedWaveform(data.data!!)
            else {
                waveformStatus.text = "Trim cancelled. No files were changed."
                trimWaveformButton.isEnabled = waveformSourceFile != null
            }
            return
        }
        if (requestCode == REQUEST_WAVEFORM) {
            wavPickerActive = false
            if (resultCode == RESULT_OK && data?.data != null) analyzeSelectedWaveform(data.data!!)
            else waveformStatus.text = "Waveform selection cancelled."
            processButton.isEnabled = !recording && !processing
            startButton.isEnabled = !recording && !processing
            waveformButton.isEnabled = !recording && !processing
            return
        }
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
        val gainDb = (dspGainSeekBar.progress - 24).toFloat()
        val highPassHz = if (highPassSeekBar.progress == 0) 0.0F else
            (20 + (highPassSeekBar.progress - 1) * 10).coerceAtMost(500).toFloat()
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
                    gainDb,
                    highPassHz,
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
        private const val WAV_HEADER_BYTES = 44L
        private const val STALE_PROCESSING_FILE_AGE_MS = 24L * 60L * 60L * 1000L
    }
}
