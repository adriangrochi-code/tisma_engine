package net.tisma.io

actual typealias ByteInput = java.io.InputStream
actual typealias ByteOutput = java.io.OutputStream
actual typealias IOException = java.io.IOException

private val secureRandom = java.security.SecureRandom()

actual fun secureRandomBytes(n: Int): ByteArray = ByteArray(n).also { secureRandom.nextBytes(it) }

actual class Lock actual constructor() {
    actual fun <T> locked(block: () -> T): T = synchronized(this) { block() }
}
