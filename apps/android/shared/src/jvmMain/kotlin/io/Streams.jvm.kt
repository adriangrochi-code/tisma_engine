package net.tisma.io

actual typealias ByteInput = java.io.InputStream
actual typealias ByteOutput = java.io.OutputStream
actual typealias IOException = java.io.IOException

private val secureRandom = java.security.SecureRandom()

actual fun secureRandomBytes(n: Int): ByteArray = ByteArray(n).also { secureRandom.nextBytes(it) }

actual class Lock actual constructor() {
    actual fun <T> locked(block: () -> T): T = synchronized(this) { block() }
}

actual fun startDaemon(name: String, block: () -> Unit) {
    Thread(block, name).apply { isDaemon = true; start() }
}

/** A socket as a [Link], buffered for reading. */
class SocketLink(val socket: java.net.Socket) : Link {
    override val input: ByteInput = socket.getInputStream().buffered()
    override val output: ByteOutput = socket.getOutputStream()
    override fun close() = socket.close()
}

actual fun sleepMs(ms: Long) = Thread.sleep(ms)
