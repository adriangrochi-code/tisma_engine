package net.tisma.app.updates

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.intOrNull
import kotlinx.serialization.json.longOrNull

/**
 * The signed release list (app_updates.md, rule 13): `updates.json`, checked
 * with its `.sig` before anything is read; the Android package the phone
 * may take (rule 2): the first, newest first, of its channel whose
 * `min_system` (the API level) the phone meets and that is newer than the
 * installed one. Own design over the spec's format.
 */
object Updates {
    data class Package(val url: String, val size: Long, val sha256: String, val signature: String, val minSystem: String)

    data class Release(val version: String, val channel: String, val date: String, val notes: Map<String, String>,
                       val packages: Map<String, Package>)

    /** What the check found: a release to offer, or why none is (AU11: too new for the phone). */
    data class Offer(val release: Release, val pkg: Package)

    data class Choice(val offer: Offer?, val needsNewerSystem: Release? = null)

    class BadList(message: String) : Exception(message)

    /** Reads the list after checking its signature with [publicKey]; a bad signature, format or version refuses it. */
    fun read(json: ByteArray, sig: ByteArray, publicKey: ByteArray): List<Release> {
        if (!Ed25519.verify(publicKey, json, sig)) throw BadList("The release list's signature does not match")
        val o = runCatching { Json.parseToJsonElement(json.toString(Charsets.UTF_8)) as JsonObject }.getOrNull()
            ?: throw BadList("The release list cannot be read")
        if ((o["format"] as? JsonPrimitive)?.contentOrNull != "tisma-updates") throw BadList("Not a release list")
        if (((o["version"] as? JsonPrimitive)?.intOrNull ?: 0) > 1) throw BadList("A newer release list format")
        fun JsonObject.s(k: String) = (this[k] as? JsonPrimitive)?.contentOrNull.orEmpty()
        return (o["releases"] as? JsonArray).orEmpty().filterIsInstance<JsonObject>().map { r ->
            val notes = (r["notes"] as? JsonObject)?.mapValues { (_, v) -> (v as? JsonPrimitive)?.contentOrNull.orEmpty() }.orEmpty()
            val pkgs = (r["packages"] as? JsonArray).orEmpty().filterIsInstance<JsonObject>()
                .filter { it.s("system") == "android" }
                .associate { p ->
                    p.s("family").ifBlank { "current" } to Package(p.s("url"), (p["size"] as? JsonPrimitive)?.longOrNull ?: -1,
                        p.s("sha256").lowercase(), p.s("signature"), p.s("min_system"))
                }
            Release(r.s("version"), r.s("channel").ifBlank { "stable" }, r.s("date"), notes, pkgs)
        }
    }

    /** Rule 13, «orden»: the first fitting release, offered only if newer than [installed]. */
    fun choose(releases: List<Release>, channel: String, sdk: Int, installed: String): Choice {
        var tooNew: Release? = null
        for (r in releases) {
            if (channel != "beta" && r.channel != "stable") continue
            val p = r.packages["current"] ?: continue
            val min = p.minSystem.toIntOrNull() ?: 0
            if (min > sdk) { if (tooNew == null && newer(r.version, installed)) tooNew = r; continue }
            return Choice(if (newer(r.version, installed)) Offer(r, p) else null, tooNew)
        }
        return Choice(null, tooNew)
    }

    /**
     * Whether [a] is newer than [b] (docs/VERSIONING.md: MAJOR.MINOR.PATCH with
     * an optional pre-release; ordered as Semantic Versioning 2.0.0, section 11).
     */
    fun newer(a: String, b: String): Boolean = compare(a, b) > 0

    fun compare(a: String, b: String): Int {
        fun split(v: String): Pair<List<Int>, List<String>?> {
            val core = v.substringBefore('+').substringBefore('-')
            val pre = v.substringBefore('+').substringAfter('-', "").takeIf { it.isNotEmpty() }?.split('.')
            return core.split('.').map { it.toIntOrNull() ?: 0 } to pre
        }
        val (ca, pa) = split(a); val (cb, pb) = split(b)
        for (i in 0 until maxOf(ca.size, cb.size)) {
            val d = (ca.getOrElse(i) { 0 }).compareTo(cb.getOrElse(i) { 0 })
            if (d != 0) return d
        }
        if (pa == null && pb == null) return 0
        if (pa == null) return 1 // a release is newer than its pre-releases
        if (pb == null) return -1
        for (i in 0 until maxOf(pa.size, pb.size)) {
            val x = pa.getOrNull(i) ?: return -1
            val y = pb.getOrNull(i) ?: return 1
            val nx = x.toIntOrNull(); val ny = y.toIntOrNull()
            val d = when {
                nx != null && ny != null -> nx.compareTo(ny)
                nx != null -> -1
                ny != null -> 1
                else -> x.compareTo(y)
            }
            if (d != 0) return d
        }
        return 0
    }

    /** The notes in the program's language, or English (rule 13, «campos»). */
    fun notes(r: Release, language: String) = r.notes[language] ?: r.notes["en"] ?: r.notes.values.firstOrNull().orEmpty()
}
