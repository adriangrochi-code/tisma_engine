package net.tisma.app.p2p

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.intOrNull
import net.tisma.app.updates.Ed25519
import java.io.File
import java.security.MessageDigest
import java.time.Duration
import java.time.Instant

/**
 * The shared library's block list (p2p_library.md, rules 4 to 6), signed with Tisma's key like the release list
 * (app_updates.md, rule 13): an unsigned or badly signed list is never used. Its format is the app's proposal
 * until the server's lands: `{"format":"tisma-blocklist","version":1,"issued":"<ISO-8601>","hashes":[sha256…],
 * "accounts":[id…],"peers":[key…],"ips":[{"ip":"…","until":"<ISO-8601>"}]}` and a detached Ed25519 `.sig`.
 */
class BlockList(
    val issued: Instant,
    val hashes: Set<String>,
    val accounts: Set<String>,
    val peers: Set<String>,
    /** Short IP blocks (rule 5: 24 h by default); an expired one is dropped when read. */
    val ips: Map<String, Instant>,
) {
    class BadList(message: String) : Exception(message)

    fun blocksFile(sha256: String) = sha256.lowercase() in hashes
    fun blocksAccount(id: String) = id in accounts
    fun blocksPeer(key: String, ip: String?, now: Instant = Instant.now()) =
        key in peers || (ip != null && ips[ip]?.isAfter(now) == true)

    /** Rule 6: a list more than 7 days old turns sharing off until it is renewed. */
    fun stale(now: Instant = Instant.now()) = Duration.between(issued, now) > MAX_AGE

    companion object {
        val MAX_AGE: Duration = Duration.ofDays(7)

        fun read(json: ByteArray, sig: ByteArray, publicKey: ByteArray, now: Instant = Instant.now()): BlockList {
            if (publicKey.size != 32 || !Ed25519.verify(publicKey, json, sig)) throw BadList("The block list's signature does not match")
            val o = runCatching { Json.parseToJsonElement(json.toString(Charsets.UTF_8)) as JsonObject }.getOrNull()
                ?: throw BadList("The block list cannot be read")
            if ((o["format"] as? JsonPrimitive)?.contentOrNull != "tisma-blocklist") throw BadList("Not a block list")
            if (((o["version"] as? JsonPrimitive)?.intOrNull ?: 0) > 1) throw BadList("A newer block list format")
            fun list(k: String) = (o[k] as? JsonArray).orEmpty().mapNotNull { (it as? JsonPrimitive)?.contentOrNull }
            val issued = runCatching { Instant.parse((o["issued"] as? JsonPrimitive)?.contentOrNull) }.getOrNull()
                ?: throw BadList("The block list has no date")
            val ips = (o["ips"] as? JsonArray).orEmpty().filterIsInstance<JsonObject>().mapNotNull { e ->
                val ip = (e["ip"] as? JsonPrimitive)?.contentOrNull ?: return@mapNotNull null
                val until = runCatching { Instant.parse((e["until"] as? JsonPrimitive)?.contentOrNull) }.getOrNull() ?: return@mapNotNull null
                (ip to until).takeIf { until.isAfter(now) } // rule 5: kept no longer than it lasts
            }.toMap()
            return BlockList(issued, list("hashes").map { it.lowercase() }.toSet(), list("accounts").toSet(), list("peers").toSet(), ips)
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
