package net.tisma.printers

import java.io.File
import java.io.InputStream
import java.security.MessageDigest
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

/** Android and the JVM: the project in a temporary file, written with java.util.zip. */
actual fun packForBambu(gcode: net.tisma.io.ByteInput): PackedProject {
    val file = File.createTempFile("bambu", ".gcode.3mf")
    val sum = try { BambuPackFile.pack(gcode, file) } catch (e: Throwable) { file.delete(); throw e }
    return object : PackedProject {
        override val length = file.length()
        override val md5 = sum
        override fun open() = file.inputStream().buffered()
        override fun delete() { file.delete() }
    }
}

object BambuPackFile {
    /**
     * Streams [gcode] into a Bambu project file at [out] (the G-code and its MD5 beside it) and returns that MD5.
     * The G-code is read once, to a temporary copy, so its MD5 is known before the ZIP is written.
     */
    fun pack(gcode: InputStream, out: File): String {
        val copy = File.createTempFile("bambu", ".gcode")
        try {
            val md = MessageDigest.getInstance("MD5")
            copy.outputStream().buffered().use { o ->
                val b = ByteArray(64 * 1024)
                while (true) { val n = gcode.read(b); if (n < 0) break; md.update(b, 0, n); o.write(b, 0, n) }
            }
            val sum = md.digest().joinToString("") { "%02x".format(it) }
            ZipOutputStream(out.outputStream().buffered()).use { z ->
                z.putNextEntry(ZipEntry("Metadata/plate_1.gcode")); copy.inputStream().use { it.copyTo(z, 64 * 1024) }; z.closeEntry()
                z.putNextEntry(ZipEntry("Metadata/plate_1.gcode.md5")); z.write(sum.toByteArray()); z.closeEntry()
            }
            return sum
        } finally {
            copy.delete()
        }
    }
}
