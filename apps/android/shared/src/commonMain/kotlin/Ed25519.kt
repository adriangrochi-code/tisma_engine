package net.tisma.app.updates

/**
 * Ed25519 signature checking (RFC 8032): only verification, the apps never sign. On the JVM and Android it is our own
 * (RFC 8032, section 5.1); on iOS it is the system's, handed in by the Swift side.
 */
expect object Ed25519 {
    fun verify(publicKey: ByteArray, message: ByteArray, signature: ByteArray): Boolean
}
