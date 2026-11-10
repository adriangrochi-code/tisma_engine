package net.tisma.app.p2p

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.booleanOrNull
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.intOrNull
import kotlinx.serialization.json.longOrNull
import net.tisma.app.updates.Ed25519

/**
 * The shared library's index (p2p_library.md, rule 3 ter, server format of 10-11-2026): `tisma-library-index` v1,
 * `generated`, `sequence` (never lower than the last seen) and `models`. Signed like the block list by a certified
 * library key. A suspended account's models and blocked files are left out (rule 4).
 */
class LibraryIndex(val sequence: Long, val models: List<Model>) {
    class BadIndex(message: String) : Exception(message)

    data class FileRef(val path: String, val role: String, val size: Long, val sha256: String)

    data class Model(
        val id: String, val revision: Int, val name: String, val description: String, val tags: List<String>,
        val license: String, val author: String, val authorAccount: String, val size: Long, val files: List<FileRef>,
        val checked: Boolean, val thumbnail: String,
    ) {
        /** Rule 3 sexies: never G-code; a model that lists one is not offered. */
        val hasGcode: Boolean get() = files.any { it.role == "gcode" || it.path.substringAfterLast('.').lowercase() in GCODE }
    }

    /** Rule 4 and the person's search (name, tag, author, license). */
    fun search(query: String, list: BlockList?): List<Model> {
        val words = query.trim().lowercase().split(Regex("\\s+")).filter { it.isNotEmpty() }
        return models.filter { m ->
            !m.hasGcode && (list == null || (!list.blocksAccount(m.authorAccount) && m.files.none { list.blocksFile(it.sha256) })) &&
                words.all { w -> (listOf(m.name, m.author, m.license) + m.tags).any { it.lowercase().contains(w) } }
        }
    }

    companion object {
        val GCODE = setOf("gcode", "bgcode", "gco", "g")

        fun read(json: ByteArray, sig: ByteArray, libraryKey: ByteArray, lastSequence: Long = -1): LibraryIndex {
            if (libraryKey.size != 32 || sig.size != 64 || !Ed25519.verify(libraryKey, json, sig)) throw BadIndex("The index's signature does not match")
            val o = runCatching { Json.parseToJsonElement(json.decodeToString()) as JsonObject }.getOrNull()
                ?: throw BadIndex("The index cannot be read")
            if ((o["format"] as? JsonPrimitive)?.contentOrNull != "tisma-library-index") throw BadIndex("Not a library index")
            if (((o["version"] as? JsonPrimitive)?.intOrNull ?: 0) > 1) throw BadIndex("A newer index format")
            val sequence = (o["sequence"] as? JsonPrimitive)?.longOrNull ?: throw BadIndex("The index has no sequence")
            if (sequence < lastSequence) throw BadIndex("An older index than the last one")
            fun JsonObject.s(k: String) = (this[k] as? JsonPrimitive)?.contentOrNull.orEmpty()
            val models = (o["models"] as? JsonArray).orEmpty().filterIsInstance<JsonObject>().map { m ->
                val author = m["author"] as? JsonObject
                Model(m.s("id"), (m["revision"] as? JsonPrimitive)?.intOrNull ?: 1, m.s("name"), m.s("description"),
                    (m["tags"] as? JsonArray).orEmpty().mapNotNull { (it as? JsonPrimitive)?.contentOrNull },
                    m.s("license"), author?.s("name").orEmpty(), author?.s("account").orEmpty(),
                    (m["size"] as? JsonPrimitive)?.longOrNull ?: 0,
                    (m["files"] as? JsonArray).orEmpty().filterIsInstance<JsonObject>().map { f ->
                        FileRef(f.s("path"), f.s("role"), (f["size"] as? JsonPrimitive)?.longOrNull ?: 0, f.s("sha256").lowercase())
                    },
                    (m["checked"] as? JsonPrimitive)?.booleanOrNull ?: false, m.s("thumbnail"))
            }
            return LibraryIndex(sequence, models)
        }
    }
}
