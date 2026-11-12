package net.tisma.app.engine

import net.tisma.io.ByteInput
import net.tisma.io.IOException
import net.tisma.io.readInto

/**
 * The analysis' colored part (mobile_app.md, rule 13, «con el mapa de colores
 * sobre la pieza»): the voxel surface the engine writes with `analyze --mesh`
 * (docs/ANALYSIS.md, «La malla de vóxeles»; PLY, Turk's public format, binary
 * little-endian), each quad as two triangles with its voxel's color (red under
 * a safety of 1, orange, yellow, green from 2, gray where nothing loads).
 * Positions are in the model file's system, moved by an offset to the shown
 * one. Read the same on Android and the iPhone. Own design.
 */
class StressMap(
    /** x y z per corner, three corners per triangle. */
    val positions: FloatArray,
    /** One color per corner, r g b from 0 to 1. */
    val colors: FloatArray,
) {
    val triangles get() = positions.size / 9

    companion object {
        /** Reads the PLY from [input] (buffered by the caller); at most [maxFaces] quads. */
        fun parse(input: ByteInput, offset: FloatArray = floatArrayOf(0f, 0f, 0f), maxFaces: Int = 1_500_000): StressMap {
            var vertices = 0
            var faces = 0
            val vertexProps = ArrayList<String>()
            val faceProps = ArrayList<String>()
            var element = ""
            // The header, line by line, up to «end_header».
            while (true) {
                val line = readLine(input).trim()
                val w = line.split(' ')
                when {
                    line == "end_header" -> break
                    w[0] == "format" && w.getOrNull(1) != "binary_little_endian" -> throw IOException("Not a binary little-endian PLY")
                    w[0] == "element" -> { element = w[1]; if (element == "vertex") vertices = w[2].toInt() else if (element == "face") faces = w[2].toInt() }
                    w[0] == "property" -> (if (element == "vertex") vertexProps else faceProps) += line
                }
            }
            if (vertexProps.size != 6 || !vertexProps[3].endsWith("red")) throw IOException("Unexpected vertex layout")
            if (faces > maxFaces) throw IOException("Too many faces for the phone ($faces)")
            val vb = ByteArray(15 * vertices)
            if (input.readInto(vb) < vb.size) throw IOException("The PLY ended early")
            val pos = FloatArray(18 * faces)
            val col = FloatArray(pos.size)
            val face = ByteArray(1 + 16 + 4 + 1)
            var o = 0
            repeat(faces) {
                if (input.readInto(face) < face.size) throw IOException("The PLY ended early")
                val n = face[0].toInt() and 0xFF
                if (n != 4) throw IOException("A face with $n corners")
                val idx = IntArray(4) { k -> int(face, 1 + 4 * k) }
                for (corner in intArrayOf(idx[0], idx[1], idx[2], idx[0], idx[2], idx[3])) {
                    if (corner !in 0 until vertices) throw IOException("A face points outside the vertices")
                    val b = 15 * corner
                    pos[o] = float(vb, b) + offset[0]; pos[o + 1] = float(vb, b + 4) + offset[1]; pos[o + 2] = float(vb, b + 8) + offset[2]
                    col[o] = (vb[b + 12].toInt() and 0xFF) / 255f
                    col[o + 1] = (vb[b + 13].toInt() and 0xFF) / 255f
                    col[o + 2] = (vb[b + 14].toInt() and 0xFF) / 255f
                    o += 3
                }
            }
            return StressMap(pos, col)
        }

        private fun int(b: ByteArray, i: Int): Int =
            (b[i].toInt() and 0xFF) or ((b[i + 1].toInt() and 0xFF) shl 8) or ((b[i + 2].toInt() and 0xFF) shl 16) or (b[i + 3].toInt() shl 24)

        private fun float(b: ByteArray, i: Int): Float = Float.fromBits(int(b, i))

        private fun readLine(input: ByteInput): String {
            val sb = StringBuilder()
            while (true) {
                val c = input.read()
                if (c < 0) throw IOException("The PLY header ended early")
                if (c == '\n'.code) return sb.toString()
                sb.append(c.toChar())
                if (sb.length > 100_000) throw IOException("A PLY header line too long")
            }
        }
    }
}
