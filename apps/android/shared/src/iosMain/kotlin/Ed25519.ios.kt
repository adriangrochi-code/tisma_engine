package net.tisma.app.updates

/**
 * On iOS the check is the system's (CryptoKit's Curve25519.Signing), which Kotlin cannot call directly: the Swift
 * side sets [verifier] at start. Until it does, nothing verifies, so nothing signed is accepted.
 */
actual object Ed25519 {
    var verifier: ((publicKey: ByteArray, message: ByteArray, signature: ByteArray) -> Boolean)? = null

    actual fun verify(publicKey: ByteArray, message: ByteArray, signature: ByteArray): Boolean =
        verifier?.invoke(publicKey, message, signature) ?: false
}
