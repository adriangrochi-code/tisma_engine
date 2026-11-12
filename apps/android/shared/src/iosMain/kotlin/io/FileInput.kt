package net.tisma.io

import kotlinx.cinterop.CPointer
import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.addressOf
import kotlinx.cinterop.usePinned
import platform.posix.FILE
import platform.posix.fclose
import platform.posix.fgetc
import platform.posix.fopen
import platform.posix.fread
import platform.posix.remove

/** A file read in order with C's stdio (iOS). With [deleteOnClose], the file is removed when closed. */
@OptIn(ExperimentalForeignApi::class)
class FileInput(private val path: String, private val deleteOnClose: Boolean = false) : ByteInput() {
    private var f: CPointer<FILE>? = fopen(path, "rb") ?: throw IOException("Cannot open $path")

    override fun read(): Int {
        val file = f ?: return -1
        val c = fgetc(file)
        return if (c < 0) -1 else c
    }

    override fun read(b: ByteArray, off: Int, len: Int): Int {
        val file = f ?: return -1
        if (len == 0) return 0
        val n = b.usePinned { p -> fread(p.addressOf(off), 1u, len.toULong(), file) }.toInt()
        return if (n == 0) -1 else n
    }

    override fun close() {
        f?.let { fclose(it) }
        f = null
        if (deleteOnClose) remove(path)
    }
}
