package net.tisma.app.engine

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import java.io.File
import kotlin.math.cos
import kotlin.math.max
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * A part's picture for «Mi biblioteca» (local_library.md, rule 1: a 256 ×
 * 256 PNG): its triangles seen from above at an angle, flat-shaded by their
 * slope to a light from the viewer, drawn back to front. Own design.
 */
object Thumbnail {
    const val SIZE = 256

    fun write(mesh: MeshFile, out: File, size: Int = SIZE): File {
        val bmp = render(mesh, size)
        out.outputStream().use { bmp.compress(Bitmap.CompressFormat.PNG, 100, it) }
        bmp.recycle()
        return out
    }

    fun render(mesh: MeshFile, size: Int = SIZE): Bitmap {
        val p = mesh.positions
        val n = p.size / 9
        // The view: turned 35° about Z, then tilted 55° down, as the plate's 3D view starts.
        val az = Math.toRadians(35.0); val el = Math.toRadians(55.0)
        val ca = cos(az).toFloat(); val sa = sin(az).toFloat(); val ce = cos(el).toFloat(); val se = sin(el).toFloat()
        val b = mesh.bounds
        val cx = (b[0] + b[3]) / 2; val cy = (b[1] + b[4]) / 2; val cz = (b[2] + b[5]) / 2
        // Screen x, screen y (up) and depth (towards the viewer) of each vertex.
        val sx = FloatArray(3 * n); val sy = FloatArray(3 * n); val sd = FloatArray(3 * n)
        for (v in 0 until 3 * n) {
            val x = p[3 * v] - cx; val y = p[3 * v + 1] - cy; val z = p[3 * v + 2] - cz
            val rx = ca * x - sa * y; val ry = sa * x + ca * y
            sx[v] = rx
            sy[v] = ce * z + se * ry
            sd[v] = se * z - ce * ry
        }
        var extent = 1e-6f
        for (v in 0 until 3 * n) extent = max(extent, max(kotlin.math.abs(sx[v]), kotlin.math.abs(sy[v])))
        val scale = size * 0.45f / extent
        val order = (0 until n).sortedBy { t -> sd[3 * t] + sd[3 * t + 1] + sd[3 * t + 2] }
        val bmp = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bmp)
        canvas.drawColor(Color.TRANSPARENT)
        val paint = Paint().apply { style = Paint.Style.FILL_AND_STROKE; strokeWidth = 0.6f; isAntiAlias = true }
        val path = Path()
        for (t in order) {
            val a = 3 * t
            // The face's normal in view space: its z is how much it faces the viewer.
            val ux = sx[a + 1] - sx[a]; val uy = sy[a + 1] - sy[a]; val ud = sd[a + 1] - sd[a]
            val vx = sx[a + 2] - sx[a]; val vy = sy[a + 2] - sy[a]; val vd = sd[a + 2] - sd[a]
            val nx = uy * vd - ud * vy; val ny = ud * vx - ux * vd; val nz = ux * vy - uy * vx
            val len = sqrt(nx * nx + ny * ny + nz * nz)
            if (len < 1e-12f) continue
            val light = 0.35f + 0.65f * kotlin.math.abs(nz / len)
            // The Tisma accent's blue-gray, lit.
            paint.color = Color.rgb((96 * light).toInt(), (150 * light).toInt(), (210 * light).toInt())
            path.reset()
            path.moveTo(size / 2f + sx[a] * scale, size / 2f - sy[a] * scale)
            path.lineTo(size / 2f + sx[a + 1] * scale, size / 2f - sy[a + 1] * scale)
            path.lineTo(size / 2f + sx[a + 2] * scale, size / 2f - sy[a + 2] * scale)
            path.close()
            canvas.drawPath(path, paint)
        }
        return bmp
    }
}
