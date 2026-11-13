package net.tisma.app.p2p

/**
 * Where the library's files come from: the server's routes without a session (rule 3 bis), and «Denunciar»
 * (rule 7). A failure means «not reachable now»: the last pieces kept on the device are used (rule 6).
 */
interface LibraryServer {
    fun file(name: String): ByteArray
    fun report(model: String?, sha256: String?, reason: String): String
    /** The server these files come from: another one (Preferences) has its own sequences and key. */
    val origin: String get() = ""
}

/** What the device keeps between runs: the last key, list and index, and the sequences seen (rule 3 ter). */
interface LibraryStore {
    fun read(name: String): ByteArray?
    fun write(name: String, bytes: ByteArray)
    fun delete(name: String)
    var listSequence: Long
    var indexSequence: Long
    /** The server the kept pieces came from; null when the device does not track it. */
    var origin: String?
}

/**
 * p2p_library.md, rule 6, the same on Android and iOS: the key (certified by [projectKey]), then the block list,
 * then the index; each fresh from [server] or, without an answer, the copy kept last time. A file and its signature
 * are taken both fresh or both kept, never mixed. A bad key, list or index throws, and nothing of it is kept.
 */
class LibraryFetch(
    private val projectKey: ByteArray,
    private val server: LibraryServer?,
    private val store: LibraryStore,
    private val maxIndex: Long = LibraryIndex.MAX_JSON,
    private val log: (String) -> Unit = {},
) {
    class Result(val list: BlockList?, val index: LibraryIndex?)

    fun refresh(): Result {
        forgetOtherServer()
        val keyJson = fetchOrKept("key") ?: return Result(null, null)
        val key = LibraryKey.read(keyJson, projectKey)
        store.write("key", keyJson)
        val list = fetchPair("blocklist.json")?.let { (json, sig) ->
            BlockList.read(json, sig, key, store.listSequence).also {
                store.write("blocklist.json", json); store.write("blocklist.json.sig", sig)
                store.listSequence = it.sequence
            }
        }
        val index = fetchPair("index.json")?.let { (json, sig) ->
            LibraryIndex.read(json, sig, key, store.indexSequence, maxIndex).also {
                store.write("index.json", json); store.write("index.json.sig", sig)
                store.indexSequence = it.sequence
            }
        }
        return Result(list, index)
    }

    private fun fetchOrKept(name: String): ByteArray? {
        val fresh = server?.let { s -> runCatching { s.file(name) }.onFailure { log("p2p: $name ${it.message}") }.getOrNull() }
        return fresh ?: store.read(name)
    }

    private fun fetchPair(name: String): Pair<ByteArray, ByteArray>? {
        val s = server
        if (s != null) {
            val fresh = runCatching { s.file(name) to s.file("$name.sig") }.onFailure { log("p2p: $name ${it.message}") }
                // Rule 3 ter bis: one too large for this version is refused with its notice, not replaced by the kept one.
                .onFailure { if (it is LibraryIndex.TooLarge) throw it }.getOrNull()
            if (fresh != null) return fresh
        }
        val json = store.read(name) ?: return null
        val sig = store.read("$name.sig") ?: return null
        return json to sig
    }

    /**
     * The person changed the cloud server (Preferences): the last server's sequences and kept files are not this
     * one's. Kept, its lists would be refused for ever as «older» than the other's.
     */
    private fun forgetOtherServer() {
        val kept = store.origin ?: return
        val origin = server?.origin.orEmpty()
        if (origin.isEmpty() || kept == origin) return
        if (kept.isNotEmpty()) {
            listOf("key", "blocklist.json", "blocklist.json.sig", "index.json", "index.json.sig").forEach { store.delete(it) }
            store.listSequence = -1
            store.indexSequence = -1
        }
        store.origin = origin
    }
}
