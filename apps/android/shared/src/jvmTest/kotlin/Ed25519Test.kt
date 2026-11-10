package net.tisma.app.updates

import java.security.KeyPairGenerator
import java.security.Signature
import kotlin.test.Test
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** The JVM's own check against signatures made by the JDK (RFC 8032). */
class Ed25519Test {
    @Test
    fun verifiesTheJdksSignaturesAndRefusesChangedOnes() {
        val keys = KeyPairGenerator.getInstance("Ed25519").generateKeyPair()
        val raw = keys.public.encoded.takeLast(32).toByteArray()
        val msg = "tisma".encodeToByteArray()
        val sig = Signature.getInstance("Ed25519").run { initSign(keys.private); update(msg); sign() }
        assertTrue(Ed25519.verify(raw, msg, sig))
        assertFalse(Ed25519.verify(raw, "tismb".encodeToByteArray(), sig))
    }
}
