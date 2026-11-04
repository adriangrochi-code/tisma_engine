package net.tisma.app.engine

import java.io.DataInputStream
import java.io.EOFException
import java.io.File
import java.io.IOException
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * The analysis' colored part (mobile_app.md, rule 13, «con el mapa de colores
 * sobre la pieza»): the voxel surface the engine writes with `analyze --mesh`
 * (docs/ANALYSIS.md, «La malla de vóxeles»; PLY, Turk's public format, binary
 * little-endian), each quad as two triangles with its voxel's color (red under
 * a safety of 1, orange, yellow, green from 2, gray where nothing loads).
 * Positions are in the model file's system, moved by [offset] to the shown
 * one. Own design.
 */
class StressMap(
    /** x y z per corner, three corners per triangle. */
    val positions: FloatArray,
    /** One color per corner, r g b from 0 to 1. */
    val colors: FloatArray,
) {
    val triangles get() = positions.size / 9

    companion object {
        fun read(file: File, offset: FloatArray = floatArrayOf(0f, 0f, 0f), maxFaces: Int = 1_500_000): StressMap {
            DataInputStream(file.inputStream().buffered(1 shl 16)).use { input ->
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
                input.readFully(vb)
                val v = ByteBuffer.wrap(vb).order(ByteOrder.LITTLE_ENDIAN)
                val pos = FloatArray(18 * faces)
                val col = FloatArray(pos.size)
                val face = ByteArray(1 + 16 + 4 + 1)
                val fb = ByteBuffer.wrap(face).order(ByteOrder.LITTLE_ENDIAN)
                var o = 0
                repeat(faces) {
                    input.readFully(face, 0, 1)
                    val n = face[0].toInt() and 0xFF
                    if (n != 4) throw IOException("A face with $n corners")
                    input.readFully(face, 1, 16 + 4 + 1)
                    val idx = IntArray(4) { k -> fb.getInt(1 + 4 * k) }
                    for (corner in intArrayOf(idx[0], idx[1], idx[2], idx[0], idx[2], idx[3])) {
                        if (corner !in 0 until vertices) throw IOException("A face points outside the vertices")
                        val b = 15 * corner
                        pos[o] = v.getFloat(b) + offset[0]; pos[o + 1] = v.getFloat(b + 4) + offset[1]; pos[o + 2] = v.getFloat(b + 8) + offset[2]
                        col[o] = (vb[b + 12].toInt() and 0xFF) / 255f
                        col[o + 1] = (vb[b + 13].toInt() and 0xFF) / 255f
                        col[o + 2] = (vb[b + 14].toInt() and 0xFF) / 255f
                        o += 3
                    }
                }
                return StressMap(pos, col)
            }
        }

        private fun readLine(input: DataInputStream): String {
            val sb = StringBuilder()
            while (true) {
                val c = input.read()
                if (c < 0) throw EOFException("The PLY header ended early")
                if (c == '\n'.code) return sb.toString()
                sb.append(c.toChar())
                if (sb.length > 100_000) throw IOException("A PLY header line too long")
            }
        }
    }
}
