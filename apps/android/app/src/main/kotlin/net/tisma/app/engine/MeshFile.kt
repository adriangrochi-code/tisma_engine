package net.tisma.app.engine

import java.io.File
import java.io.InputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.zip.ZipFile
import kotlin.math.sqrt

/**
 * A model's triangles for the plate's 3D view (mobile_app.md, rule 6): positions
 * as one float array (x y z per corner, three corners per triangle) and one
 * normal per triangle. Read from STL (binary and ASCII, the format's public
 * description), OBJ (vertices and faces, polygons as fans) and 3MF (the 3MF
 * core specification: the build items' objects, their components and
 * transforms). Only for showing: the engine reads the model itself to slice.
 */
class MeshFile(
    val positions: FloatArray,
    val normals: FloatArray,
    /** How far the model was moved from its file's coordinates (x, y, z), to give regions back in them. */
    val offset: FloatArray = floatArrayOf(0f, 0f, 0f),
) {
    val triangles get() = positions.size / 9

    /** min x, y, z, max x, y, z. */
    val bounds: FloatArray by lazy {
        val b = floatArrayOf(Float.MAX_VALUE, Float.MAX_VALUE, Float.MAX_VALUE, -Float.MAX_VALUE, -Float.MAX_VALUE, -Float.MAX_VALUE)
        var i = 0
        while (i < positions.size) {
            for (a in 0..2) {
                val v = positions[i + a]
                if (v < b[a]) b[a] = v
                if (v > b[a + 3]) b[a + 3] = v
            }
            i += 3
        }
        if (positions.isEmpty()) floatArrayOf(0f, 0f, 0f, 1f, 1f, 1f) else b
    }

    /** The same model moved so it stands on the bed, centered on (cx, cy). */
    fun onBed(cx: Float, cy: Float): MeshFile {
        val b = bounds
        val dx = cx - (b[0] + b[3]) / 2
        val dy = cy - (b[1] + b[4]) / 2
        val dz = -b[2]
        val p = positions.copyOf()
        var i = 0
        while (i < p.size) { p[i] += dx; p[i + 1] += dy; p[i + 2] += dz; i += 3 }
        return MeshFile(p, normals, floatArrayOf(offset[0] + dx, offset[1] + dy, offset[2] + dz))
    }

    companion object {
        fun read(file: File): MeshFile = when (file.extension.lowercase()) {
            "stl" -> readStl(file)
            "obj" -> file.inputStream().use { readObj(it) }
            "3mf" -> read3mf(file)
            else -> throw IllegalArgumentException("Not a model: ${file.name}")
        }

        private class Builder {
            var pos = FloatArray(9 * 1024)
            var n = 0
            fun add(ax: Float, ay: Float, az: Float, bx: Float, by: Float, bz: Float, cx: Float, cy: Float, cz: Float) {
                if (9 * (n + 1) > pos.size) pos = pos.copyOf(pos.size * 2)
                val o = 9 * n
                pos[o] = ax; pos[o + 1] = ay; pos[o + 2] = az
                pos[o + 3] = bx; pos[o + 4] = by; pos[o + 5] = bz
                pos[o + 6] = cx; pos[o + 7] = cy; pos[o + 8] = cz
                n++
            }

            fun build(): MeshFile {
                val p = pos.copyOf(9 * n)
                val normals = FloatArray(3 * n)
                for (t in 0 until n) {
                    val o = 9 * t
                    val ux = p[o + 3] - p[o]; val uy = p[o + 4] - p[o + 1]; val uz = p[o + 5] - p[o + 2]
                    val vx = p[o + 6] - p[o]; val vy = p[o + 7] - p[o + 1]; val vz = p[o + 8] - p[o + 2]
                    var nx = uy * vz - uz * vy; var ny = uz * vx - ux * vz; var nz = ux * vy - uy * vx
                    val len = sqrt(nx * nx + ny * ny + nz * nz)
                    if (len > 0) { nx /= len; ny /= len; nz /= len }
                    normals[3 * t] = nx; normals[3 * t + 1] = ny; normals[3 * t + 2] = nz
                }
                return MeshFile(p, normals)
            }
        }

        /** Binary STL when its size matches the triangle count; else ASCII. */
        fun readStl(file: File): MeshFile {
            val len = file.length()
            if (len >= 84) {
                val head = file.inputStream().use { input -> ByteArray(84).also { readFully(input, it) } }
                val count = ByteBuffer.wrap(head, 80, 4).order(ByteOrder.LITTLE_ENDIAN).int.toLong() and 0xFFFFFFFFL
                if (84 + count * 50 == len) return file.inputStream().buffered().use { readBinaryStl(it, count.toInt()) }
            }
            return file.bufferedReader().use { r ->
                val b = Builder()
                val v = FloatArray(9)
                var k = 0
                r.forEachLine { line ->
                    val t = line.trim()
                    if (t.startsWith("vertex", ignoreCase = true)) {
                        val parts = t.split(Regex("\\s+"))
                        if (parts.size >= 4 && k < 3) {
                            v[3 * k] = parts[1].toFloat(); v[3 * k + 1] = parts[2].toFloat(); v[3 * k + 2] = parts[3].toFloat()
                            k++
                        }
                    } else if (t.startsWith("endloop", ignoreCase = true)) {
                        if (k == 3) b.add(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8])
                        k = 0
                    }
                }
                b.build()
            }
        }

        private fun readBinaryStl(input: InputStream, count: Int): MeshFile {
            val head = ByteArray(84)
            readFully(input, head)
            val b = Builder()
            val rec = ByteArray(50)
            val bb = ByteBuffer.wrap(rec).order(ByteOrder.LITTLE_ENDIAN)
            repeat(count) {
                readFully(input, rec)
                b.add(bb.getFloat(12), bb.getFloat(16), bb.getFloat(20), bb.getFloat(24), bb.getFloat(28), bb.getFloat(32),
                    bb.getFloat(36), bb.getFloat(40), bb.getFloat(44))
            }
            return b.build()
        }

        fun readObj(input: InputStream): MeshFile {
            val verts = ArrayList<FloatArray>()
            val b = Builder()
            input.bufferedReader().forEachLine { line ->
                val t = line.trim()
                when {
                    t.startsWith("v ") -> t.split(Regex("\\s+")).let { verts += floatArrayOf(it[1].toFloat(), it[2].toFloat(), it[3].toFloat()) }
                    t.startsWith("f ") -> {
                        val idx = t.split(Regex("\\s+")).drop(1).map { s ->
                            val i = s.substringBefore('/').toInt()
                            if (i < 0) verts.size + i else i - 1
                        }
                        for (k in 1 until idx.size - 1) {
                            val a = verts[idx[0]]; val c = verts[idx[k]]; val d = verts[idx[k + 1]]
                            b.add(a[0], a[1], a[2], c[0], c[1], c[2], d[0], d[1], d[2])
                        }
                    }
                }
            }
            return b.build()
        }

        /** 3MF: every build item's object (and its components), with the item's and the components' transforms. */
        fun read3mf(file: File): MeshFile = ZipFile(file).use { z ->
            val entry = z.entries().asSequence().firstOrNull { it.name.startsWith("3D/") && it.name.endsWith(".model") }
                ?: throw IllegalArgumentException("No model in ${file.name}")
            val xml = z.getInputStream(entry).bufferedReader().readText()
            val objects = mutableMapOf<String, Pair<FloatArray, IntArray>>()
            val components = mutableMapOf<String, List<Pair<String, FloatArray>>>()
            Regex("""<object\b([^>]*)>(.*?)</object>""", RegexOption.DOT_MATCHES_ALL).findAll(xml).forEach { m ->
                val id = attr(m.groupValues[1], "id") ?: return@forEach
                val body = m.groupValues[2]
                if (body.contains("<mesh")) {
                    val vs = ArrayList<Float>()
                    Regex("""<vertex\b([^>]*)/?>""").findAll(body).forEach { v ->
                        vs += attr(v.groupValues[1], "x")!!.toFloat(); vs += attr(v.groupValues[1], "y")!!.toFloat()
                        vs += attr(v.groupValues[1], "z")!!.toFloat()
                    }
                    val ts = ArrayList<Int>()
                    Regex("""<triangle\b([^>]*)/?>""").findAll(body).forEach { t ->
                        ts += attr(t.groupValues[1], "v1")!!.toInt(); ts += attr(t.groupValues[1], "v2")!!.toInt()
                        ts += attr(t.groupValues[1], "v3")!!.toInt()
                    }
                    objects[id] = vs.toFloatArray() to ts.toIntArray()
                }
                val comps = Regex("""<component\b([^>]*)/?>""").findAll(body).mapNotNull { c ->
                    val oid = attr(c.groupValues[1], "objectid") ?: return@mapNotNull null
                    oid to matrix(attr(c.groupValues[1], "transform"))
                }.toList()
                if (comps.isNotEmpty()) components[id] = comps
            }
            val b = Builder()
            fun emit(id: String, m: FloatArray, depth: Int) {
                if (depth > 16) return
                objects[id]?.let { (v, t) ->
                    var i = 0
                    while (i + 2 < t.size) {
                        val p = FloatArray(9)
                        for (k in 0..2) {
                            val vi = 3 * t[i + k]
                            val x = v[vi]; val y = v[vi + 1]; val zz = v[vi + 2]
                            // 3MF: row vectors; [x y z 1] times the 4x3 matrix.
                            p[3 * k] = x * m[0] + y * m[3] + zz * m[6] + m[9]
                            p[3 * k + 1] = x * m[1] + y * m[4] + zz * m[7] + m[10]
                            p[3 * k + 2] = x * m[2] + y * m[5] + zz * m[8] + m[11]
                        }
                        b.add(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8])
                        i += 3
                    }
                }
                components[id]?.forEach { (cid, cm) -> emit(cid, compose(cm, m), depth + 1) }
            }
            val build = Regex("""<item\b([^>]*)/?>""").findAll(xml).toList()
            if (build.isEmpty()) objects.keys.forEach { emit(it, identity(), 0) }
            build.forEach { item ->
                val oid = attr(item.groupValues[1], "objectid") ?: return@forEach
                emit(oid, matrix(attr(item.groupValues[1], "transform")), 0)
            }
            b.build()
        }

        private fun attr(attrs: String, name: String): String? =
            Regex("""\b$name\s*=\s*"([^"]*)"""").find(attrs)?.groupValues?.get(1)

        private fun identity() = floatArrayOf(1f, 0f, 0f, 0f, 1f, 0f, 0f, 0f, 1f, 0f, 0f, 0f)

        private fun matrix(text: String?): FloatArray {
            val v = text?.trim()?.split(Regex("\\s+"))?.mapNotNull { it.toFloatOrNull() }
            return if (v != null && v.size == 12) v.toFloatArray() else identity()
        }

        /** a then b, as 3MF's row-vector 4x3 matrices. */
        private fun compose(a: FloatArray, b: FloatArray): FloatArray {
            val r = FloatArray(12)
            for (row in 0..3) for (col in 0..2) {
                var s = if (row == 3) b[9 + col] else 0f
                for (k in 0..2) s += a[3 * row + k] * b[3 * k + col]
                r[3 * row + col] = s
            }
            return r
        }

        private fun readFully(input: InputStream, b: ByteArray) {
            var got = 0
            while (got < b.size) {
                val n = input.read(b, got, b.size - got)
                if (n < 0) throw java.io.EOFException()
                got += n
            }
        }
    }
}
