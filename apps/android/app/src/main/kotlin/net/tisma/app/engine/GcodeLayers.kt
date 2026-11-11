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
 * (`;LAYER_CHANGE`, `;Z:`, `;HEIGHT:`, `;TYPE:`, `;WIDTH:`) and the moves
 * themselves (G0/G1 with their feed rate, G2/G3 arcs as short chords,
 * G90/G91, M82/M83, G92). Travel moves are left out. Each layer keeps its
 * segments in one float array (x0 y0 x1 y1) and their width (mm, 0 when the
 * G-code does not say) and speed (mm/s) in two more, so a large print stays
 * small in memory. Own design.
 */
class GcodeLayers private constructor(val layers: List<Layer>, val types: List<String>) {

    class Layer(
        val z: Float, val height: Float, val segments: FloatArray, val segmentTypes: ByteArray,
        val widths: FloatArray, val speeds: FloatArray,
    ) {
        val count get() = segmentTypes.size
    }

    /** The smallest and largest width (mm) of the extrusions, or null when the G-code gives none. */
    val widthRange: ClosedFloatingPointRange<Float>? by lazy { range { it.widths } }

    /** The smallest and largest speed (mm/s) of the extrusions, or null when the G-code gives none. */
    val speedRange: ClosedFloatingPointRange<Float>? by lazy { range { it.speeds } }

    /**
     * The extrusions of layers 0..[upTo] as 3D lines for the 3D view: two
     * vertices per segment, each x y z r g b, at the layer's Z. [rgb] gives a
     * segment's color as 0xRRGGBB. At most [maxSegments] are kept, the top
     * layers first (the ones seen; the top one always), so a large print stays drawable.
     */
    fun lines3d(upTo: Int, maxSegments: Int = 1_500_000, rgb: (Layer, Int) -> Int): FloatArray {
        val last = upTo.coerceIn(-1, layers.lastIndex)
        var first = last + 1
        var total = 0
        while (first > 0 && (first == last + 1 || total + layers[first - 1].count <= maxSegments)) { first--; total += layers[first].count }
        val out = FloatArray(12 * total)
        var o = 0
        for (li in first..last) {
            val l = layers[li]
            val s = l.segments
            for (i in 0 until l.count) {
                val c = rgb(l, i)
                val r = (c shr 16 and 0xFF) / 255f; val g = (c shr 8 and 0xFF) / 255f; val bl = (c and 0xFF) / 255f
                for (k in 0..1) {
                    out[o] = s[4 * i + 2 * k]; out[o + 1] = s[4 * i + 2 * k + 1]; out[o + 2] = l.z
                    out[o + 3] = r; out[o + 4] = g; out[o + 5] = bl
                    o += 6
                }
            }
        }
        return out
    }

    private fun range(values: (Layer) -> FloatArray): ClosedFloatingPointRange<Float>? {
        var lo = Float.MAX_VALUE; var hi = -Float.MAX_VALUE
        for (l in layers) for (v in values(l)) if (v > 0) { if (v < lo) lo = v; if (v > hi) hi = v }
        return if (lo > hi) null else lo..hi
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
        var widths = FloatArray(256)
        var speeds = FloatArray(256)
        var n = 0

        fun add(x0: Float, y0: Float, x1: Float, y1: Float, type: Int, width: Float, speed: Float) {
            if (4 * (n + 1) > seg.size) seg = seg.copyOf(seg.size * 2)
            if (n + 1 > types.size) {
                types = types.copyOf(types.size * 2)
                widths = widths.copyOf(types.size)
                speeds = speeds.copyOf(types.size)
            }
            seg[4 * n] = x0; seg[4 * n + 1] = y0; seg[4 * n + 2] = x1; seg[4 * n + 3] = y1
            types[n] = type.toByte()
            widths[n] = width
            speeds[n] = speed
            n++
        }

        fun build() = Layer(z, height, seg.copyOf(4 * n), types.copyOf(n), widths.copyOf(n), speeds.copyOf(n))
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
        var width = 0f
        /** The feed rate in mm/min, kept from move to move as G-code does. */
        var feed = 0.0

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
                    t.startsWith(";WIDTH:") -> width = t.substring(7).trim().toFloatOrNull() ?: width
                }
                return
            }
            val (word, a) = words(t.substringBefore(';'))
            when (word) {
                "G90" -> absolute = true
                "G91" -> absolute = false
                "M82" -> absoluteE = true
                "M83" -> absoluteE = false
                "G92" -> a['E']?.let { e = it }
                "G0", "G1", "G00", "G01" -> move(a)
                "G2", "G3", "G02", "G03" -> arc(a, clockwise = word == "G2" || word == "G02")
            }
        }

        /**
         * The command (`G1`) and its arguments. Words are a letter and a number, with or without spaces between
         * them (`G1X10Y5E.4` is the same as `G1 X10 Y5 E.4`); a line number (`N12`) and a checksum (`*71`) are
         * skipped.
         */
        fun words(code: String): Pair<String, Map<Char, Double>> {
            val out = HashMap<Char, Double>(6)
            var command = ""
            var i = 0
            while (i < code.length) {
                val c = code[i]
                if (c == '*') break
                if (!c.isLetter()) { i++; continue }
                var j = i + 1
                while (j < code.length && (code[j] == ' ' || code[j] == '\t') && j + 1 < code.length && !code[j + 1].isLetter()) j++
                val start = j
                while (j < code.length && (code[j].isDigit() || code[j] == '.' || code[j] == '-' || code[j] == '+')) j++
                val letter = c.uppercaseChar()
                val number = code.substring(start, j)
                if (command.isEmpty() && (letter == 'G' || letter == 'M' || letter == 'T')) command = letter + number
                else if (letter != 'N') number.toDoubleOrNull()?.let { out[letter] = it }
                i = j
            }
            return command to out
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
            a['F']?.let { feed = it }
            val de = extrude(a)
            if (de > 1e-9 && (nx != x || ny != y)) {
                if (type < 0) type = typeIndex("Custom")
                layerFor(nz).add(x.toFloat(), y.toFloat(), nx.toFloat(), ny.toFloat(), type, width, (feed / 60).toFloat())
            }
            x = nx; y = ny; z = nz
        }

        fun arc(a: Map<Char, Double>, clockwise: Boolean) {
            val nx = a['X']?.let { if (absolute) it else x + it } ?: x
            val ny = a['Y']?.let { if (absolute) it else y + it } ?: y
            // A helical arc (spiral vase with arcs) climbs: its Z is the end's.
            val nz = a['Z']?.let { if (absolute) it else z + it } ?: z
            var cx = x + (a['I'] ?: 0.0)
            var cy = y + (a['J'] ?: 0.0)
            // The radius form (`R`, Marlin's G2/G3): the centre on the chord's bisector; a negative R is the long way.
            val radius = a['R']
            if (radius != null && a['I'] == null && a['J'] == null) {
                val dx = nx - x; val dy = ny - y
                val d = hypot(dx, dy)
                val rr = abs(radius)
                if (d < 1e-9 || d > 2 * rr + 1e-6) { x = nx; y = ny; z = nz; extrude(a); return }
                val h = kotlin.math.sqrt((rr * rr - d * d / 4).coerceAtLeast(0.0))
                // Left of the chord for counter-clockwise short arcs; the other side for clockwise or the long way.
                val side = (if (clockwise) -1.0 else 1.0) * (if (radius < 0) -1.0 else 1.0)
                cx = (x + nx) / 2 - side * h * dy / d
                cy = (y + ny) / 2 + side * h * dx / d
            }
            a['F']?.let { feed = it }
            val de = extrude(a)
            val r = hypot(x - cx, y - cy)
            var a0 = atan2(y - cy, x - cx)
            var a1 = atan2(ny - cy, nx - cx)
            if (clockwise && a1 >= a0) a1 -= 2 * Math.PI
            if (!clockwise && a1 <= a0) a1 += 2 * Math.PI
            if (de > 1e-9 && r > 1e-6) {
                val steps = ceil(abs(a1 - a0) / (Math.PI / 36)).toInt().coerceIn(1, 360)
                val layer = layerFor(nz)
                if (type < 0) type = typeIndex("Custom")
                var px = x; var py = y
                for (k in 1..steps) {
                    val ang = a0 + (a1 - a0) * k / steps
                    val qx = if (k == steps) nx else cx + r * cos(ang)
                    val qy = if (k == steps) ny else cy + r * sin(ang)
                    layer.add(px.toFloat(), py.toFloat(), qx.toFloat(), qy.toFloat(), type, width, (feed / 60).toFloat())
                    px = qx; py = qy
                }
            }
            x = nx; y = ny; z = nz
        }
    }
}
