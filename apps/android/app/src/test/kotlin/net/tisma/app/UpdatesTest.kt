package net.tisma.app

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import net.tisma.app.updates.Ed25519
import net.tisma.app.updates.Updates
import org.junit.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The release list and its signatures (app_updates.md, rules 2, 5 and 13; AU2, AU3, AU9 to AU11). */
class UpdatesTest {
    private fun hex(s: String) = ByteArray(s.length / 2) { s.substring(2 * it, 2 * it + 2).toInt(16).toByte() }

    /** Signatures made by another implementation (the `cryptography` package for Python), as test data. */
    private val vectors = Json.parseToJsonElement(javaClass.getResource("/ed25519_vectors.json")!!.readText()).jsonArray.map {
        val o = it.jsonObject
        Triple(hex(o["pub"]!!.jsonPrimitive.content), hex(o["msg"]!!.jsonPrimitive.content), hex(o["sig"]!!.jsonPrimitive.content))
    }

    @Test
    fun `Ed25519 accepts good signatures and refuses a changed byte or another key`() {
        for ((pub, msg, sig) in vectors) {
            assertTrue(Ed25519.verify(pub, msg, sig))
            // AU2: one byte changed in the message; AU3: another key; a changed signature.
            if (msg.isNotEmpty()) assertFalse(Ed25519.verify(pub, msg.copyOf().also { it[0] = (it[0].toInt() xor 1).toByte() }, sig))
            assertFalse(Ed25519.verify(vectors.first { it.first !== pub }.first, msg, sig))
            assertFalse(Ed25519.verify(pub, msg, sig.copyOf().also { it[5] = (it[5].toInt() xor 4).toByte() }))
        }
    }

    @Test
    fun `versions compare as docs VERSIONING md says`() {
        assertTrue(Updates.newer("0.10.0", "0.9.9"))
        assertTrue(Updates.newer("1.0.0", "1.0.0-beta.2"))
        assertTrue(Updates.newer("1.0.0-beta.10", "1.0.0-beta.2"))
        assertTrue(Updates.newer("1.0.0-beta.1", "1.0.0-alpha.3"))
        assertFalse(Updates.newer("0.1.0", "0.1.0"))
    }

    @Test
    fun `the list is read only with its signature, and the right release is chosen`() {
        val (pub, _, _) = vectors[0]
        // A list signed with a test key by the same other implementation (updates_signed.json).
        val signed = Json.parseToJsonElement(javaClass.getResource("/updates_signed.json")!!.readText()).jsonObject
        val key = hex(signed["pub"]!!.jsonPrimitive.content)
        val json = signed["json"]!!.jsonPrimitive.content.toByteArray()
        val sig = hex(signed["sig"]!!.jsonPrimitive.content)
        val releases = Updates.read(json, sig, key)
        assertEquals(listOf("1.1.0-beta.1", "1.0.1", "1.0.0"), releases.map { it.version })
        assertEquals("ab", releases[1].packages["current"]!!.sha256)
        // AU10: a byte changed with the old signature.
        assertFailsWith<Updates.BadList> { Updates.read(json.copyOf().also { it[10] = 'X'.code.toByte() }, sig, key) }
        assertFailsWith<Updates.BadList> { Updates.read(json, sig, pub) }
        // Stable on API 31 with 1.0.0: 1.0.1; beta: 1.1.0-beta.1 (AU9).
        assertEquals("1.0.1", Updates.choose(releases, "stable", 31, "1.0.0").offer!!.release.version)
        assertEquals("1.1.0-beta.1", Updates.choose(releases, "beta", 31, "1.0.0").offer!!.release.version)
        assertNull(Updates.choose(releases, "stable", 31, "1.0.1").offer)
        // AU11: 1.0.1 asks API 31; on API 28 the phone keeps 1.0.0 and is told why.
        val old = Updates.choose(releases, "stable", 28, "1.0.0")
        assertNull(old.offer)
        assertEquals("1.0.1", old.needsNewerSystem!!.version)
        assertEquals("Arreglos", Updates.notes(releases[1], "es"))
        assertEquals("Fixes", Updates.notes(releases[1], "fr"))
    }
}
