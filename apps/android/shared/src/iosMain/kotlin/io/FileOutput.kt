package net.tisma.io

import kotlinx.cinterop.CPointer
import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.addressOf
import kotlinx.cinterop.usePinned
import platform.posix.FILE
import platform.posix.fclose
import platform.posix.fflush
import platform.posix.fopen
import platform.posix.fwrite

/** A file written in order with C's stdio (iOS). */
@OptIn(ExperimentalForeignApi::class)
class FileOutput(path: String) : ByteOutput() {
    private var f: CPointer<FILE>? = fopen(path, "wb") ?: throw IOException("Cannot write $path")
    var written = 0L
        private set

    override fun write(b: Int) = write(byteArrayOf(b.toByte()), 0, 1)

    override fun write(b: ByteArray, off: Int, len: Int) {
        if (len == 0) return
        val file = f ?: throw IOException("Closed")
        val n = b.usePinned { p -> fwrite(p.addressOf(off), 1u, len.toULong(), file) }.toInt()
        if (n != len) throw IOException("The phone's storage is full")
        written += n
    }

    override fun flush() { f?.let { fflush(it) } }

    override fun close() {
        f?.let { if (fclose(it) != 0) { f = null; throw IOException("The phone's storage is full") } }
        f = null
    }
}
