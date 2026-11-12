package net.tisma.printers

import kotlinx.cinterop.CPointer
import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.addressOf
import kotlinx.cinterop.usePinned
import net.tisma.io.ByteInput
import net.tisma.io.ByteOutput
import net.tisma.io.Crc32
import net.tisma.io.FileInput
import net.tisma.io.FileOutput
import net.tisma.io.IOException
import net.tisma.io.Md5Digest
import net.tisma.io.StoredEntry
import net.tisma.io.hex
import net.tisma.io.writeStoredZip
import platform.Foundation.NSTemporaryDirectory
import platform.posix.FILE
import platform.posix.fclose
import platform.posix.fflush
import platform.posix.fopen
import platform.posix.fwrite
import platform.posix.remove

/**
 * iOS: the G-code is copied once to a temporary file while its MD5 and CRC-32 are worked out, then packed with our
 * stored ZIP writer into a second one, so neither is ever whole in memory.
 */
actual fun packForBambu(gcode: ByteInput): PackedProject {
    val base = NSTemporaryDirectory() + "bambu-" + kotlin.random.Random.nextLong().toULong().toString(16)
    val copy = "$base.gcode"
    val packed = "$base.gcode.3mf"
    try {
        val md = Md5Digest(); val crc = Crc32()
        var size = 0L
        FileOutput(copy).use { o ->
            val b = ByteArray(64 * 1024)
            while (true) {
                val n = gcode.read(b, 0, b.size)
                if (n < 0) break
                md.update(b, 0, n); crc.update(b, 0, n); o.write(b, 0, n); size += n
            }
        }
        val sum = hex(md.digest())
        val sumBytes = sum.encodeToByteArray()
        val sumCrc = Crc32().apply { update(sumBytes, 0, sumBytes.size) }.value
        var length = 0L
        FileOutput(packed).use { o ->
            writeStoredZip(o, listOf(
                StoredEntry("Metadata/plate_1.gcode", size, crc.value) { FileInput(copy) },
                StoredEntry("Metadata/plate_1.gcode.md5", sumBytes.size.toLong(), sumCrc) { net.tisma.io.BytesInput(sumBytes) },
            ))
            length = o.written
        }
        remove(copy)
        return object : PackedProject {
            override val length = length
            override val md5 = sum
            override fun open(): ByteInput = FileInput(packed)
            override fun delete() { remove(packed) }
        }
    } catch (e: Throwable) {
        remove(copy); remove(packed)
        throw e
    }
}
