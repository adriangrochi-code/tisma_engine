package net.tisma.app.p2p

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.intOrNull
import kotlinx.serialization.json.longOrNull
import net.tisma.app.updates.Ed25519
import java.io.File
import java.security.MessageDigest
import java.time.Instant
import java.util.Base64

/**
 * The library's key (p2p_library.md, rule 3 quinquies): the app carries only the project's key; the server's
 * library key is accepted when the project key signed `tisma-library-key/1\n` + its 32 bytes (the certificate) and
 * it is not in the list's `revoked_keys`. No certificate (a test server): not used.
 */
object LibraryKey {
    class BadKey(message: String) : Exception(message)

    private val PREFIX = "tisma-library-key/1\n".toByteArray()

    /** `GET /v1/library/key`: `{"public_key": …, "certificate": …}`, each in base64 (or hex). */
    fun read(json: ByteArray, projectKey: ByteArray): ByteArray {
        if (projectKey.size != 32) throw BadKey("No project key in this build")
        val o = runCatching { Json.parseToJsonElement(json.toString(Charsets.UTF_8)) as JsonObject }.getOrNull()
            ?: throw BadKey("The library key cannot be read")
        val key = bytes(o["public_key"]) ?: throw BadKey("No library key")
        val cert = bytes(o["certificate"]) ?: throw BadKey("The library key has no certificate")
        if (key.size != 32 || cert.size != 64) throw BadKey("A library key of the wrong size")
        if (!Ed25519.verify(projectKey, PREFIX + key, cert)) throw BadKey("The library key's certificate does not match")
        return key
    }

    internal fun bytes(e: JsonElement?): ByteArray? {
        val s = (e as? JsonPrimitive)?.contentOrNull?.trim()?.takeIf { it.isNotEmpty() } ?: return null
        if (s.length % 2 == 0 && s.all { it in "0123456789abcdefABCDEF" }) return ByteArray(s.length / 2) { s.substring(2 * it, 2 * it + 2).toInt(16).toByte() }
        return runCatching { Base64.getDecoder().decode(s) }.getOrNull()
    }

    fun hex(b: ByteArray) = b.joinToString("") { "%02x".format(it) }
}

/**
 * The shared library's block list (p2p_library.md, rules 3 quater and 4 to 6, server format of 10-11-2026):
 * `format` `tisma-library-blocklist`, `version` 1, `generated`, `sequence` (a lower one than the last seen is
 * refused), `expires` (generated + 7 days, signed again every 6 h), `files` (`sha256`, `reason`), `accounts`,
 * `peers`, `addresses` (hashed, for the server's own use: the phone goes through Tisma's relays, rule 8 ter) and
 * `revoked_keys`. Signed by a certified library key with a 64-byte `.sig` over the exact bytes.
 */
class BlockList(
    val generated: Instant,
    val sequence: Long,
    val expires: Instant,
    /** sha256 → reason. */
    val files: Map<String, String>,
    val accounts: Set<String>,
    val peers: Set<String>,
    val revokedKeys: Set<String>,
) {
    class BadList(message: String) : Exception(message)

    fun blocksFile(sha256: String) = sha256.lowercase() in files
    fun blocksAccount(id: String) = id in accounts
    fun blocksPeer(key: String) = key in peers

    /** Rule 6: past `expires` (7 days without a new signature), sharing stops until it is renewed. */
    fun stale(now: Instant = Instant.now()) = now.isAfter(expires)

    companion object {
        /**
         * Reads a list signed by [libraryKey]. [lastSequence] is the highest seen before (rule 3 ter: a list that
         * goes back is refused, so an old one cannot be replayed).
         */
        fun read(json: ByteArray, sig: ByteArray, libraryKey: ByteArray, lastSequence: Long = -1): BlockList {
            if (libraryKey.size != 32 || sig.size != 64 || !Ed25519.verify(libraryKey, json, sig)) throw BadList("The block list's signature does not match")
            val o = runCatching { Json.parseToJsonElement(json.toString(Charsets.UTF_8)) as JsonObject }.getOrNull()
                ?: throw BadList("The block list cannot be read")
            if ((o["format"] as? JsonPrimitive)?.contentOrNull != "tisma-library-blocklist") throw BadList("Not a block list")
            if (((o["version"] as? JsonPrimitive)?.intOrNull ?: 0) > 1) throw BadList("A newer block list format")
            val sequence = (o["sequence"] as? JsonPrimitive)?.longOrNull ?: throw BadList("The block list has no sequence")
            if (sequence < lastSequence) throw BadList("An older block list than the last one ($sequence < $lastSequence)")
            fun time(k: String) = runCatching { Instant.parse((o[k] as? JsonPrimitive)?.contentOrNull) }.getOrNull()
            val generated = time("generated") ?: throw BadList("The block list has no date")
            val expires = time("expires") ?: generated.plusSeconds(7 * 24 * 3600)
            fun strings(k: String) = (o[k] as? JsonArray).orEmpty().mapNotNull { e ->
                (e as? JsonPrimitive)?.contentOrNull ?: ((e as? JsonObject)?.get("id") as? JsonPrimitive)?.contentOrNull
                    ?: ((e as? JsonObject)?.get("key") as? JsonPrimitive)?.contentOrNull
            }.toSet()
            val files = (o["files"] as? JsonArray).orEmpty().mapNotNull { e ->
                when (e) {
                    is JsonPrimitive -> e.contentOrNull?.lowercase()?.let { it to "" }
                    is JsonObject -> (e["sha256"] as? JsonPrimitive)?.contentOrNull?.lowercase()
                        ?.let { it to ((e["reason"] as? JsonPrimitive)?.contentOrNull ?: "") }
                    else -> null
                }
            }.toMap()
            val list = BlockList(generated, sequence, expires, files, strings("accounts"), strings("peers"),
                strings("revoked_keys").map { it.lowercase() }.toSet())
            if (LibraryKey.hex(libraryKey) in list.revokedKeys) throw BadList("The library key that signed it is revoked")
            return list
        }

        fun sha256(f: File): String {
            val md = MessageDigest.getInstance("SHA-256")
            f.inputStream().use { i -> val b = ByteArray(64 * 1024); while (true) { val n = i.read(b); if (n < 0) break; md.update(b, 0, n) } }
            return md.digest().joinToString("") { "%02x".format(it) }
        }
    }
}

/**
 * The shared library's files on this phone (p2p_library.md, rule 2): only what the person published or downloaded,
 * each named by its SHA-256. «Mi biblioteca» is not here and is never shared (rule 8).
 */
class SharedCopies(val dir: File) {
    data class Copy(val sha256: String, val file: File)

    fun all(): List<Copy> = dir.listFiles().orEmpty().filter { it.isFile }.map { Copy(it.nameWithoutExtension.lowercase(), it) }

    /** Keeps [file] under its SHA-256 (checked, not trusted from its name). */
    fun add(file: File, extension: String): Copy {
        dir.mkdirs()
        val sha = BlockList.sha256(file)
        val target = File(dir, "$sha.$extension")
        file.copyTo(target, overwrite = true)
        return Copy(sha, target)
    }

    /** Rule 4: a blocked file is not kept: deleted, and returned so the person is told. */
    fun apply(list: BlockList): List<Copy> = all().filter { list.blocksFile(it.sha256) }.onEach { it.file.delete() }
}

/** Rules 3, 6 and P2P6: when the phone may share (upload). Downloading is separate and only when asked. */
object SharingPolicy {
    enum class Why { OFF, NO_LIST, STALE_LIST, NOT_WIFI, NOT_CHARGING }

    /** Null when sharing may run; else the first reason it may not. */
    fun blocked(enabled: Boolean, list: BlockList?, unmeteredNetwork: Boolean, charging: Boolean, now: Instant = Instant.now()): Why? = when {
        !enabled -> Why.OFF
        list == null -> Why.NO_LIST
        list.stale(now) -> Why.STALE_LIST
        !unmeteredNetwork -> Why.NOT_WIFI
        !charging -> Why.NOT_CHARGING
        else -> null
    }
}
