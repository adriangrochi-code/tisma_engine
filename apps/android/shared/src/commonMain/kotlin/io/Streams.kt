package net.tisma.io

/**
 * Bytes read in order, as the cloud and printer clients stream them (an upload, a result of up to 1 GB). On the JVM
 * and Android this is `java.io.InputStream` itself, so the Android code is unchanged; on iOS it is our own class.
 */
expect abstract class ByteInput() : AutoCloseable {
    /** One byte (0 to 255), or -1 at the end. */
    abstract fun read(): Int
    /** Up to [len] bytes into [b] from [off]; how many, or -1 at the end. */
    open fun read(b: ByteArray, off: Int, len: Int): Int
    override fun close()
}

/** Bytes written in order. `java.io.OutputStream` on the JVM and Android. */
expect abstract class ByteOutput() : AutoCloseable {
    abstract fun write(b: Int)
    open fun write(b: ByteArray, off: Int, len: Int)
    open fun flush()
    override fun close()
}

/** A failure to read or write, or no answer from the network. `java.io.IOException` on the JVM and Android. */
expect open class IOException(message: String?) : Exception

/** Reads into all of [b] when it can; how many were read (fewer only at the end). */
fun ByteInput.readInto(b: ByteArray): Int {
    var n = 0
    while (n < b.size) {
        val r = read(b, n, b.size - n)
        if (r < 0) break
        n += r
    }
    return n
}

/** Copies to the end; how many bytes. */
fun ByteInput.copyAll(out: ByteOutput): Long {
    val buf = ByteArray(64 * 1024)
    var total = 0L
    while (true) {
        val r = read(buf, 0, buf.size)
        if (r < 0) break
        out.write(buf, 0, r)
        total += r
    }
    return total
}

/** The bytes of [b], read in order. */
class BytesInput(private val b: ByteArray) : ByteInput() {
    private var pos = 0
    override fun read(): Int = if (pos < b.size) b[pos++].toInt() and 0xFF else -1
    override fun read(b: ByteArray, off: Int, len: Int): Int {
        if (len == 0) return 0
        if (pos >= this.b.size) return -1
        val n = minOf(len, this.b.size - pos)
        this.b.copyInto(b, off, pos, pos + n)
        pos += n
        return n
    }
    override fun close() {}
}

/** Collects what is written. */
class BytesOutput : ByteOutput() {
    private var buf = ByteArray(256)
    private var n = 0
    override fun write(b: Int) { grow(1); buf[n++] = b.toByte() }
    override fun write(b: ByteArray, off: Int, len: Int) { grow(len); b.copyInto(buf, n, off, off + len); n += len }
    private fun grow(more: Int) { if (n + more > buf.size) buf = buf.copyOf(maxOf(buf.size * 2, n + more)) }
    fun toByteArray(): ByteArray = buf.copyOf(n)
    override fun flush() {}
    override fun close() {}
}

/** A lock for state that two threads may touch (a printer polled while the screen sends a command). */
expect class Lock() {
    fun <T> locked(block: () -> T): T
}

/** A two-way byte link (a TCP or TLS socket): what the MQTT and FTP clients talk over. */
interface Link : AutoCloseable {
    val input: ByteInput
    val output: ByteOutput
}

/** Runs [block] on a background thread of its own that does not keep the app alive. */
expect fun startDaemon(name: String, block: () -> Unit)

/** The wall clock in milliseconds. */
fun nowMs(): Long = kotlin.time.Clock.System.now().toEpochMilliseconds()
