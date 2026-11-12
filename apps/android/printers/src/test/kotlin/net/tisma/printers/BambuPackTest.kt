package net.tisma.printers

import java.io.File
import java.io.InputStream
import java.security.MessageDigest
import java.util.zip.ZipFile
import kotlin.test.Test
import kotlin.test.assertEquals

/** A long print's G-code is packed for a Bambu printer without holding it in memory (the tests run in 256 MB). */
class BambuPackTest {
    /** [size] bytes of G-code made on the fly. */
    private class Gcode(val size: Long) : InputStream() {
        private val line = "G1 X10.000 Y20.000 E0.01234\n".toByteArray()
        private var pos = 0L
        override fun read(): Int = if (pos >= size) -1 else line[(pos++ % line.size).toInt()].toInt() and 0xFF
        override fun read(b: ByteArray, off: Int, len: Int): Int {
            if (pos >= size) return -1
            val n = minOf(len.toLong(), size - pos).toInt()
            for (i in 0 until n) b[off + i] = line[((pos + i) % line.size).toInt()]
            pos += n
            return n
        }
    }

    @Test
    fun `a 400 MB G-code is packed with its MD5 in a small heap`() {
        val size = 400L * 1024 * 1024
        val out = File.createTempFile("pack", ".3mf")
        try {
            val sum = BambuPackFile.pack(Gcode(size), out)
            val md = MessageDigest.getInstance("MD5")
            Gcode(size).use { i -> val b = ByteArray(1 shl 16); while (true) { val n = i.read(b); if (n < 0) break; md.update(b, 0, n) } }
            assertEquals(md.digest().joinToString("") { "%02x".format(it) }, sum)
            ZipFile(out).use { z ->
                assertEquals(size, z.getEntry("Metadata/plate_1.gcode").size)
                assertEquals(sum, z.getInputStream(z.getEntry("Metadata/plate_1.gcode.md5")).readBytes().decodeToString())
            }
        } finally {
            out.delete()
        }
    }
}
