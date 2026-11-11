package net.tisma.io

actual abstract class ByteInput actual constructor() : AutoCloseable {
    actual abstract fun read(): Int
    actual open fun read(b: ByteArray, off: Int, len: Int): Int {
        var n = 0
        while (n < len) {
            val c = read()
            if (c < 0) return if (n == 0) -1 else n
            b[off + n] = c.toByte()
            n++
        }
        return n
    }
    actual override fun close() {}
}

actual abstract class ByteOutput actual constructor() : AutoCloseable {
    actual abstract fun write(b: Int)
    actual open fun write(b: ByteArray, off: Int, len: Int) { for (i in off until off + len) write(b[i].toInt()) }
    actual open fun flush() {}
    actual override fun close() {}
}

actual open class IOException actual constructor(message: String?) : Exception(message)
