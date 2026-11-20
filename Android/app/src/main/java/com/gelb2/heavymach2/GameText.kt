package com.gelb2.heavymach2

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Typeface
import android.text.Layout
import android.text.StaticLayout
import android.text.TextPaint
import android.util.Log
import java.nio.ByteBuffer
import kotlin.math.ceil

internal object GameText {
    private val faces = mutableMapOf<String, Typeface>()

    // The requested family and weight come from the common UI tables.
    // Android supplies its sans-serif family and its own Korean glyph fallback.
    fun rasterize(value: String, font: String, size: Float, width: Int, height: Int,
                  contentWidth: Float, contentHeight: Float, align: Int): ByteArray {
        val face = faces.getOrPut(font) {
            val style = when {
                font.contains("BoldItalic", true) -> Typeface.BOLD_ITALIC
                font.contains("Bold", true) -> Typeface.BOLD
                font.contains("Italic", true) -> Typeface.ITALIC
                else -> Typeface.NORMAL
            }
            Log.i("HeavyMach2", "Android text family mapping: $font to sans-serif, style $style")
            Typeface.create("sans-serif", style)
        }
        val paint = TextPaint(TextPaint.ANTI_ALIAS_FLAG or TextPaint.SUBPIXEL_TEXT_FLAG).apply {
            typeface = face
            textSize = size
            color = Color.WHITE
        }
        val alignment = when (align) {
            1 -> Layout.Alignment.ALIGN_CENTER
            2 -> Layout.Alignment.ALIGN_OPPOSITE
            else -> Layout.Alignment.ALIGN_NORMAL
        }
        val text = StaticLayout.Builder.obtain(value, 0, value.length, paint,
            ceil(contentWidth).toInt().coerceAtLeast(1))
            .setAlignment(alignment).setIncludePad(false)
            .setBreakStrategy(Layout.BREAK_STRATEGY_SIMPLE).build()
        val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ALPHA_8)
        try {
            val canvas = Canvas(bitmap)
            canvas.clipRect(0f, 0f, contentWidth, contentHeight)
            text.draw(canvas)
            val storage = ByteBuffer.allocate(bitmap.byteCount)
            bitmap.copyPixelsToBuffer(storage)
            val bytes = storage.array()
            if (bitmap.rowBytes == width) return bytes
            val alpha = ByteArray(width * height)
            for (y in 0 until height) bytes.copyInto(alpha, y * width, y * bitmap.rowBytes, y * bitmap.rowBytes + width)
            return alpha
        } finally { bitmap.recycle() }
    }
}
