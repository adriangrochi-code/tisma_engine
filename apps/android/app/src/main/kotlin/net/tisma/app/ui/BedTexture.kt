package net.tisma.app.ui

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import java.io.File

/**
 * A bed's picture from the profile pack (profile_pack.md, rule 14) as a
 * bitmap for the 3D view, at most [maxPx] on its longer side: PNG with
 * Android's decoder, SVG drawn with AndroidSVG. Null when it cannot be read.
 */
object BedTexture {
    fun load(file: File, maxPx: Int = 1024): Bitmap? = runCatching {
        when (file.extension.lowercase()) {
            "png" -> {
                val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
                BitmapFactory.decodeFile(file.path, bounds)
                var sample = 1
                while (maxOf(bounds.outWidth, bounds.outHeight) / sample > maxPx) sample *= 2
                BitmapFactory.decodeFile(file.path, BitmapFactory.Options().apply { inSampleSize = sample })
            }
            "svg" -> {
                val svg = file.inputStream().use { com.caverock.androidsvg.SVG.getFromInputStream(it) }
                val box = svg.documentViewBox
                val w0 = box?.width() ?: svg.documentWidth.takeIf { it > 0 } ?: maxPx.toFloat()
                val h0 = box?.height() ?: svg.documentHeight.takeIf { it > 0 } ?: maxPx.toFloat()
                val scale = maxPx / maxOf(w0, h0)
                val w = (w0 * scale).toInt().coerceAtLeast(1); val h = (h0 * scale).toInt().coerceAtLeast(1)
                // The picture fills the bitmap whatever size the file says ("100%" in the pack's files).
                svg.setDocumentWidth(w.toFloat()); svg.setDocumentHeight(h.toFloat())
                Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888).also { svg.renderToCanvas(Canvas(it)) }
            }
            else -> null
        }
    }.getOrNull()
}
