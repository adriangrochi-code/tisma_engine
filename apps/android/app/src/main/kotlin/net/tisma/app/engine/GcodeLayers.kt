package net.tisma.app.engine

import java.io.BufferedReader
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.ceil
import kotlin.math.cos
import kotlin.math.hypot
import kotlin.math.sin

/**
 * The extrusions of a G-code, layer by layer, for the preview (mobile_app.md,
 * rule 12; cloud.md, rule 9). Read from the tags of docs/spec/GCODE_OUTPUT.md
 * (`;LAYER_CHANGE`, `;Z:`, `;HEIGHT:`, `;TYPE:`) and the moves themselves
 * (G0/G1, G2/G3 arcs as short chords, G90/G91, M82/M83, G92). Travel moves
 * are left out. Each layer keeps its segments in one float array (x0 y0 x1 y1),
 * so a large print stays small in memory. Own design.
 */
class GcodeLayers private constructor(val layers: List<Layer>, val types: List<String>) {

    class Layer(val z: Float, val height: Float, val segments: FloatArray, val segmentTypes: ByteArray) {
        val count get() = segmentTypes.size
    }

    /** The extent of all extrusions, for fitting the view. */
    val bounds: FloatArray by lazy {
        var minX = Float.MAX_VALUE; var minY = Float.MAX_VALUE; var maxX = -Float.MAX_VALUE; var maxY = -Float.MAX_VALUE
        for (l in layers) {
            var i = 0
            while (i < l.segments.size) {
                val x = l.segments[i]; val y = l.segments[i + 1]
                if (x < minX) minX = x; if (x > maxX) maxX = x
                if (y < minY) minY = y; if (y > maxY) maxY = y
                i += 2
            }
        }
        if (minX > maxX) floatArrayOf(0f, 0f, 1f, 1f) else floatArrayOf(minX, minY, maxX, maxY)
    }

    companion object {
        /** The binary G-code format starts with these bytes; read through [BinaryGcode]. */
        val BINARY_MAGIC = byteArrayOf('G'.code.toByte(), 'C'.code.toByte(), 'D'.code.toByte(), 'E'.code.toByte())

        fun read(reader: BufferedReader): GcodeLayers = Parser().run(reader)
    }

    private class Builder(val z: Float, var height: Float) {
        var seg = FloatArray(1024)
        var types = ByteArray(256)
        var n = 0

        fun add(x0: Float, y0: Float, x1: Float, y1: Float, type: Int) {
            if (4 * (n + 1) > seg.size) seg = seg.copyOf(seg.size * 2)
            if (n + 1 > types.size) types = types.copyOf(types.size * 2)
            seg[4 * n] = x0; seg[4 * n + 1] = y0; seg[4 * n + 2] = x1; seg[4 * n + 3] = y1
            types[n] = type.toByte()
            n++
        }

        fun build() = Layer(z, height, seg.copyOf(4 * n), types.copyOf(n))
    }

    private class Parser {
        val layers = mutableListOf<Layer>()
        val types = mutableListOf<String>()
        var current: Builder? = null
        var type = -1
        var x = 0.0; var y = 0.0; var z = 0.0; var e = 0.0
        var absolute = true
        var absoluteE = true
        var tagZ: Float? = null
        var tagHeight = 0f

        fun typeIndex(name: String): Int {
            val i = types.indexOf(name)
            if (i >= 0) return i
            types += name
            return types.size - 1
        }

        fun layerFor(zNow: Double): Builder {
            val c = current
            val target = tagZ ?: zNow.toFloat()
            if (c != null && abs(c.z - target) < 1e-4) return c
            c?.takeIf { it.n > 0 }?.let { layers += it.build() }
            val prevZ = layers.lastOrNull()?.z ?: 0f
            return Builder(target, if (tagHeight > 0) tagHeight else (target - prevZ)).also { current = it }
        }

        fun run(reader: BufferedReader): GcodeLayers {
            reader.forEachLine { raw -> line(raw) }
            current?.takeIf { it.n > 0 }?.let { layers += it.build() }
            return GcodeLayers(layers, types)
        }

        fun line(raw: String) {
            val t = raw.trim()
            if (t.isEmpty()) return
            if (t[0] == ';') {
                when {
                    t.startsWith(";LAYER_CHANGE") -> { tagZ = null; tagHeight = 0f }
                    t.startsWith(";Z:") -> tagZ = t.substring(3).trim().toFloatOrNull()
                    t.startsWith(";HEIGHT:") -> tagHeight = t.substring(8).trim().toFloatOrNull() ?: 0f
                    t.startsWith(";TYPE:") -> type = typeIndex(t.substring(6).trim())
                }
                return
            }
            val code = t.substringBefore(';').trim()
            val word = code.substringBefore(' ').uppercase()
            when (word) {
                "G90" -> absolute = true
                "G91" -> absolute = false
                "M82" -> absoluteE = true
                "M83" -> absoluteE = false
                "G92" -> args(code)['E']?.let { e = it }
                "G0", "G1", "G00", "G01" -> move(args(code))
                "G2", "G3", "G02", "G03" -> arc(args(code), clockwise = word == "G2" || word == "G02")
            }
        }

        fun args(code: String): Map<Char, Double> {
            val out = HashMap<Char, Double>(6)
            var i = code.indexOf(' ')
            while (i in 0 until code.length) {
                while (i < code.length && code[i] == ' ') i++
                if (i >= code.length) break
                val letter = code[i].uppercaseChar()
                var j = i + 1
                while (j < code.length && code[j] != ' ') j++
                code.substring(i + 1, j).toDoubleOrNull()?.let { out[letter] = it }
                i = j
            }
            return out
        }

        /** How much the move extrudes, and updates E. */
        fun extrude(a: Map<Char, Double>): Double {
            val v = a['E'] ?: return 0.0
            return if (absoluteE) (v - e).also { e = v } else v
        }

        fun move(a: Map<Char, Double>) {
            val nx = a['X']?.let { if (absolute) it else x + it } ?: x
            val ny = a['Y']?.let { if (absolute) it else y + it } ?: y
            val nz = a['Z']?.let { if (absolute) it else z + it } ?: z
            val de = extrude(a)
            if (de > 1e-9 && (nx != x || ny != y)) {
                layerFor(nz).add(x.toFloat(), y.toFloat(), nx.toFloat(), ny.toFloat(), type.coerceAtLeast(0).also {
                    if (type < 0) type = typeIndex("Custom")
                })
            }
            x = nx; y = ny; z = nz
        }

        fun arc(a: Map<Char, Double>, clockwise: Boolean) {
            val nx = a['X']?.let { if (absolute) it else x + it } ?: x
            val ny = a['Y']?.let { if (absolute) it else y + it } ?: y
            val cx = x + (a['I'] ?: 0.0)
            val cy = y + (a['J'] ?: 0.0)
            val de = extrude(a)
            val r = hypot(x - cx, y - cy)
            var a0 = atan2(y - cy, x - cx)
            var a1 = atan2(ny - cy, nx - cx)
            if (clockwise && a1 >= a0) a1 -= 2 * Math.PI
            if (!clockwise && a1 <= a0) a1 += 2 * Math.PI
            if (de > 1e-9 && r > 1e-6) {
                val steps = ceil(abs(a1 - a0) / (Math.PI / 36)).toInt().coerceIn(1, 360)
                val layer = layerFor(z)
                if (type < 0) type = typeIndex("Custom")
                var px = x; var py = y
                for (k in 1..steps) {
                    val ang = a0 + (a1 - a0) * k / steps
                    val qx = if (k == steps) nx else cx + r * cos(ang)
                    val qy = if (k == steps) ny else cy + r * sin(ang)
                    layer.add(px.toFloat(), py.toFloat(), qx.toFloat(), qy.toFloat(), type)
                    px = qx; py = qy
                }
            }
            x = nx; y = ny
        }
    }
}
