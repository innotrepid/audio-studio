package com.innotrepid.audiostudio

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.view.View
import kotlin.math.max
import kotlin.math.min

class WaveformView(context: Context) : View(context) {
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = 0xFF9FE7B5.toInt()
        strokeWidth = 2f
        style = Paint.Style.STROKE
    }
    private val centerPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = 0xFF38404D.toInt()
        strokeWidth = 1f
    }
    private val selectionFill = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = 0x333FA9F5
        style = Paint.Style.FILL
    }
    private val selectionMarker = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = 0xFFFFC857.toInt()
        strokeWidth = 2f
    }
    private var selectionStart = 0f
    private var selectionEnd = 1f
    private var peaks = FloatArray(0)

    fun setSelection(startFraction: Float, endFraction: Float) {
        selectionStart = startFraction.coerceIn(0f, 1f)
        selectionEnd = endFraction.coerceIn(selectionStart, 1f)
        invalidate()
    }

    fun setPeaks(values: FloatArray) {
        peaks = values.copyOf()
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val center = height / 2f
        canvas.drawColor(0xFF171D27.toInt())
        if (width <= 0 || height <= 0) return
        val startX = selectionStart * width
        val endX = selectionEnd * width
        canvas.drawRect(startX, 0f, endX, height.toFloat(), selectionFill)
        canvas.drawLine(0f, center, width.toFloat(), center, centerPaint)
        if (peaks.size < 4) return
        val buckets = peaks.size / 4
        val amplitude = max(1f, center - 6f)
        for (i in 0 until buckets) {
            val minPeak = min(peaks[i * 4], peaks[i * 4 + 2]).coerceIn(-1f, 1f)
            val maxPeak = max(peaks[i * 4 + 1], peaks[i * 4 + 3]).coerceIn(-1f, 1f)
            val x = (i.toFloat() / buckets) * width
            canvas.drawLine(x, center - maxPeak * amplitude, x, center - minPeak * amplitude, paint)
        }
        canvas.drawLine(startX, 0f, startX, height.toFloat(), selectionMarker)
        canvas.drawLine(endX, 0f, endX, height.toFloat(), selectionMarker)
    }
}
