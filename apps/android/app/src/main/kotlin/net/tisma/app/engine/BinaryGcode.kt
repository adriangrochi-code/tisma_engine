package net.tisma.app.engine

import java.io.EOFException
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.io.SequenceInputStream
import java.util.Enumeration
import java.util.zip.CRC32

/**
 * The text G-code inside a binary G-code, as the engine writes it
 * (docs/spec/GCODE_OUTPUT.md; the engine's own reader in src/gcode/binary.cpp):
 * little-endian, "GCDE", version 1, a checksum type (0 none, 1 CRC32), then
 * blocks of type, compression, data size, parameters, data and, with CRC32,
 * the block's checksum. The G-code is the G-code blocks' data in order.
 * Compressed blocks are not read (the engine does not write them). Own design.
 */
object BinaryGcode {
    class Unreadable(message: String) : IOException(message)

    private const val GCODE = 1
    private const val THUMBNAIL = 5
    /** The largest block read (64 MB). */
    const val MAX_BLOCK = 64L * 1024 * 1024

    /** The text G-code, read block by block as it is consumed (a large print is not held whole in memory). */
    fun open(file: File): InputStream {
        val input = file.inputStream().buffered()
        try {
            val head = ByteArray(10)
            readFully(input, head)
            if (!head.copyOf(4).contentEquals(GcodeLayers.BINARY_MAGIC)) throw Unreadable("Not a binary G-code")
            if (le(head, 4, 4) != 1L) throw Unreadable("Binary G-code version ${le(head, 4, 4)} is not 1")
            val checksum = le(head, 8, 2)
            if (checksum > 1) throw Unreadable("Unknown checksum type $checksum")
            return SequenceInputStream(Blocks(input, checksum == 1L))
        } catch (e: Exception) {
            input.close()
            throw e
        }
    }

    private class Blocks(val input: InputStream, val crc: Boolean) : Enumeration<InputStream> {
        private var next: ByteArray? = advance()

        override fun hasMoreElements() = next != null

        override fun nextElement(): InputStream {
            val b = next ?: throw NoSuchElementException()
            next = advance()
            return b.inputStream()
        }

        /** The next G-code block's data, or null at the end (the stream is then closed). */
        private fun advance(): ByteArray? {
            while (true) {
                val header = ByteArray(8)
                val got = readSome(input, header)
                if (got == 0) { input.close(); return null }
                if (got < 8) throw Unreadable("A block header cut short")
                val type = le(header, 0, 2).toInt()
                if (le(header, 2, 2) != 0L) throw Unreadable("Compressed blocks are not read")
                if (type > THUMBNAIL) throw Unreadable("Unknown block type $type")
                val size = le(header, 4, 4)
                // Real blocks are tens of KB; a damaged or hostile header must not take the phone's memory.
                if (size > MAX_BLOCK) throw Unreadable("A block is too large")
                val params = ByteArray(if (type == THUMBNAIL) 6 else 2)
                val data = ByteArray(size.toInt())
                readFully(input, params)
                readFully(input, data)
                if (crc) {
                    val stored = ByteArray(4)
                    readFully(input, stored)
                    val c = CRC32()
                    c.update(header); c.update(params); c.update(data)
                    if (c.value != le(stored, 0, 4)) throw Unreadable("A block's CRC32 does not match")
                }
                if (type == GCODE) return data
            }
        }
    }

    private fun le(b: ByteArray, at: Int, n: Int): Long {
        var v = 0L
        for (i in n - 1 downTo 0) v = (v shl 8) or (b[at + i].toLong() and 0xFF)
        return v
    }

    private fun readSome(input: InputStream, b: ByteArray): Int {
        var got = 0
        while (got < b.size) {
            val n = input.read(b, got, b.size - got)
            if (n < 0) break
            got += n
        }
        return got
    }

    private fun readFully(input: InputStream, b: ByteArray) {
        if (readSome(input, b) < b.size) throw EOFException("A block runs past the end of the file")
    }
}
