package net.tisma.app.engine

import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.roundToLong
import kotlin.math.sqrt

/**
 * The geometry of touching a face of the part (mobile_app.md, rule 13), on a triangle soup: positions as x y z per
 * corner, three corners per triangle. The flat face around a touched triangle (the triangles joined to it that lie in
 * its plane), the box around it, its normal, and the engine's own face name when it is the part's end in an axis
 * (docs/ANALYSIS.md, «El caso»). The same on Android and the iPhone. Own design.
 */
object FacePicking {
    /** One unit normal per triangle (zero for a degenerate one). */
    fun normals(p: FloatArray): FloatArray {
        val n = FloatArray(p.size / 3)
        for (t in 0 until p.size / 9) {
            val o = 9 * t
            val ux = p[o + 3] - p[o]; val uy = p[o + 4] - p[o + 1]; val uz = p[o + 5] - p[o + 2]
            val vx = p[o + 6] - p[o]; val vy = p[o + 7] - p[o + 1]; val vz = p[o + 8] - p[o + 2]
            val nx = uy * vz - uz * vy; val ny = uz * vx - ux * vz; val nz = ux * vy - uy * vx
            val len = sqrt(nx * nx + ny * ny + nz * nz)
            if (len > 0f) { n[3 * t] = nx / len; n[3 * t + 1] = ny / len; n[3 * t + 2] = nz / len }
        }
        return n
    }

    /** min x, y, z, max x, y, z (a unit box when there is nothing). */
    fun bounds(p: FloatArray): FloatArray {
        if (p.isEmpty()) return floatArrayOf(0f, 0f, 0f, 1f, 1f, 1f)
        val b = floatArrayOf(Float.MAX_VALUE, Float.MAX_VALUE, Float.MAX_VALUE, -Float.MAX_VALUE, -Float.MAX_VALUE, -Float.MAX_VALUE)
        var i = 0
        while (i < p.size) {
            for (a in 0..2) {
                val v = p[i + a]
                if (v < b[a]) b[a] = v
                if (v > b[a + 3]) b[a + 3] = v
            }
            i += 3
        }
        return b
    }

    /**
     * The flat face of [seed]: the triangles reached through shared corners whose normal is within [maxAngleDeg] of
     * the seed's and whose corners lie within [tolerance] mm of its plane.
     */
    fun flatFace(p: FloatArray, n: FloatArray, seed: Int, maxAngleDeg: Double = 5.0, tolerance: Float = 0.05f): IntArray {
        val triangles = p.size / 9
        val byCorner = HashMap<Long, MutableList<Int>>()
        fun key(i: Int): Long {
            val q = 1000f
            val x = (p[i] * q).roundToLong(); val y = (p[i + 1] * q).roundToLong(); val z = (p[i + 2] * q).roundToLong()
            return (x * 73856093L) xor (y * 19349663L) xor (z * 83492791L)
        }
        for (t in 0 until triangles) for (k in 0..2) byCorner.getOrPut(key(9 * t + 3 * k)) { ArrayList(6) }.add(t)
        val sx = n[3 * seed]; val sy = n[3 * seed + 1]; val sz = n[3 * seed + 2]
        val planeD = sx * p[9 * seed] + sy * p[9 * seed + 1] + sz * p[9 * seed + 2]
        val cosMax = cos(maxAngleDeg * PI / 180.0).toFloat()
        fun fits(t: Int): Boolean {
            if (n[3 * t] * sx + n[3 * t + 1] * sy + n[3 * t + 2] * sz < cosMax) return false
            for (k in 0..2) {
                val i = 9 * t + 3 * k
                if (abs(sx * p[i] + sy * p[i + 1] + sz * p[i + 2] - planeD) > tolerance) return false
            }
            return true
        }
        val seen = BooleanArray(triangles)
        val out = ArrayList<Int>()
        val queue = ArrayDeque<Int>()
        queue += seed; seen[seed] = true
        while (queue.isNotEmpty()) {
            val t = queue.removeFirst()
            out += t
            for (k in 0..2) for (o in byCorner[key(9 * t + 3 * k)].orEmpty()) {
                if (!seen[o] && fits(o)) { seen[o] = true; queue += o }
            }
        }
        return out.toIntArray()
    }

    /** The box around [triangles] (min x y z, max x y z), [pad] mm larger on every side. */
    fun box(p: FloatArray, triangles: IntArray, pad: Float): FloatArray {
        val b = floatArrayOf(Float.MAX_VALUE, Float.MAX_VALUE, Float.MAX_VALUE, -Float.MAX_VALUE, -Float.MAX_VALUE, -Float.MAX_VALUE)
        for (t in triangles) for (k in 0..2) for (a in 0..2) {
            val v = p[9 * t + 3 * k + a]
            if (v < b[a]) b[a] = v
            if (v > b[a + 3]) b[a + 3] = v
        }
        for (a in 0..2) { b[a] -= pad; b[a + 3] += pad }
        return b
    }

    /** The area-weighted normal of [triangles], unit length. */
    fun normal(p: FloatArray, triangles: IntArray): FloatArray {
        var x = 0f; var y = 0f; var z = 0f
        for (t in triangles) {
            val i = 9 * t
            val ux = p[i + 3] - p[i]; val uy = p[i + 4] - p[i + 1]; val uz = p[i + 5] - p[i + 2]
            val vx = p[i + 6] - p[i]; val vy = p[i + 7] - p[i + 1]; val vz = p[i + 8] - p[i + 2]
            x += uy * vz - uz * vy; y += uz * vx - ux * vz; z += ux * vy - uy * vx
        }
        val l = sqrt(x * x + y * y + z * z).takeIf { it > 0 } ?: 1f
        return floatArrayOf(x / l, y / l, z / l)
    }

    /** The engine's face name when [triangles] (normal [n]) lie on the part's end in an axis ([bounds]), or null. */
    fun endFace(p: FloatArray, bounds: FloatArray, triangles: IntArray, n: FloatArray): String? {
        val axis = (0..2).firstOrNull { abs(n[it]) > 0.996f } ?: return null
        val max = n[axis] > 0
        val target = if (max) bounds[axis + 3] else bounds[axis]
        for (t in triangles) for (k in 0..2) if (abs(p[9 * t + 3 * k + axis] - target) > 0.01f) return null
        return when (axis) {
            0 -> if (max) "x_max" else "x_min"
            1 -> if (max) "y_max" else "y_min"
            else -> if (max) "top" else "bottom"
        }
    }
}
