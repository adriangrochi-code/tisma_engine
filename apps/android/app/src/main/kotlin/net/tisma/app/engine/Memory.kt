package net.tisma.app.engine

import java.io.File
import java.util.zip.ZipFile

/**
 * Whether a model fits the phone's memory before slicing (mobile_app.md, rules 12
 * and 19; MA9). Until the engine's own estimate exists (its C API), the app counts
 * the triangles and uses a design factor, to be corrected when measured.
 */
object MemoryEstimate {
    /**
     * Bytes the engine needs per triangle while slicing. Measured on 09-11-2026 (host build, 4 threads, a
     * sphere of 0.1 to 2 million triangles, with and without supports and 40 % infill): about 85 MB plus 90 bytes
     * per triangle at the peak (2 million with supports: 270 MB). Taken with more than three times the margin
     * for more threads on a phone and parts with more paths. The earlier design value, 3000 bytes, asked 3 GB for
     * a million triangles and turned mid-range phones away.
     */
    const val BYTES_PER_TRIANGLE = 300L
    /** Fixed part: the engine itself and its buffers (measured about 85 MB; with margin). */
    const val BASE_BYTES = 128L * 1024 * 1024

    fun triangles(model: File): Long = when (model.extension.lowercase()) {
        "stl" -> stlTriangles(model)
        "3mf" -> threeMfTriangles(model)
        "obj" -> model.bufferedReader().useLines { lines -> lines.count { it.startsWith("f ") }.toLong() }
        else -> 0
    }

    fun needed(triangles: Long): Long = BASE_BYTES + triangles * BYTES_PER_TRIANGLE

    /** Binary STL: the count after the 80-byte header, if the size matches; else ASCII facets. */
    private fun stlTriangles(f: File): Long {
        if (f.length() >= 84) {
            val head = f.inputStream().use { it.readUpToCompat(84) }
            val n = (head[80].toLong() and 0xFF) or ((head[81].toLong() and 0xFF) shl 8) or
                ((head[82].toLong() and 0xFF) shl 16) or ((head[83].toLong() and 0xFF) shl 24)
            if (84 + n * 50 == f.length()) return n
        }
        return f.bufferedReader().useLines { lines -> lines.count { it.trimStart().startsWith("facet") }.toLong() }
    }

    private fun threeMfTriangles(f: File): Long = runCatching {
        ZipFile(f).use { z ->
            z.entries().asSequence().filter { it.name.startsWith("3D/") && it.name.endsWith(".model") }.sumOf { e ->
                var count = 0L
                z.getInputStream(e).bufferedReader().use { r ->
                    val buf = CharArray(64 * 1024)
                    var carry = ""
                    while (true) {
                        val n = r.read(buf)
                        if (n < 0) break
                        val text = carry + String(buf, 0, n)
                        var i = text.indexOf("<triangle ")
                        while (i >= 0) { count++; i = text.indexOf("<triangle ", i + 1) }
                        carry = text.takeLast(9)
                    }
                }
                count
            }
        }
    }.getOrDefault(0)

    private fun java.io.InputStream.readUpToCompat(n: Int): ByteArray {
        val b = ByteArray(n)
        var got = 0
        while (got < n) { val r = read(b, got, n - got); if (r < 0) break; got += r }
        return b
    }
}
