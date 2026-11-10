package net.tisma.app.p2p

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import net.tisma.app.data.AppLog
import java.io.File

/** The shared library's screen (p2p_library.md). */
data class SharedState(
    val list: BlockList? = null,
    val index: LibraryIndex? = null,
    /** Why the last key, list or index was not used. */
    val problem: String? = null,
    /** Rule 4: copies deleted because the list withdrew them, to tell the person. */
    val withdrawn: List<String> = emptyList(),
    val copies: List<SharedCopies.Copy> = emptyList(),
    val query: String = "",
    val sharing: Boolean = false,
    val uploadCapMb: Int = 500,
    val unmetered: Boolean = false,
    val charging: Boolean = false,
    /** «Denunciar» sent (the model's or file's id), or why it failed. */
    val reported: String? = null,
    val reportFailed: String? = null,
) {
    val blockedBy: SharingPolicy.Why? get() = SharingPolicy.blocked(sharing, list, unmetered, charging)
    val found: List<LibraryIndex.Model> get() = index?.search(query, list).orEmpty()
}

/**
 * Where the library's files come from: the server's routes without a session (rule 3 bis), and «Denunciar»
 * (rule 7). Null pieces mean «not reachable now»: the last ones kept on the phone are used (rule 6).
 */
interface LibraryServer {
    fun file(name: String): ByteArray
    fun report(model: String?, sha256: String?, reason: String): String
}

/**
 * [projectKey] is the project's Ed25519 key (BuildConfig.UPDATES_KEY); it certifies the library key (rule 3
 * quinquies). Empty until the owner's key exists: then nothing is accepted and nothing is shared.
 */
class SharedViewModel(
    private val dir: File,
    private val projectKey: ByteArray,
    private val server: LibraryServer? = null,
    private val saved: net.tisma.app.data.AppSettings? = null,
    private val io: CoroutineDispatcher = Dispatchers.IO,
) : ViewModel() {
    private val copies = SharedCopies(File(dir, "files"))
    private val _state = MutableStateFlow(SharedState(sharing = saved?.p2pSharing ?: false, uploadCapMb = saved?.p2pUploadCapMb ?: 500))
    val state: StateFlow<SharedState> = _state.asStateFlow()

    private fun kept(name: String) = File(dir, name)

    /** Fetches [name] (and keeps it) or, without an answer, the copy kept from last time. */
    private fun fetchOrKept(name: String): ByteArray? {
        val fresh = server?.let { s -> runCatching { s.file(name) }.onFailure { AppLog.line("p2p: $name ${it.message}") }.getOrNull() }
        return fresh ?: kept(name).takeIf { it.isFile }?.readBytes()
    }

    private fun keep(name: String, b: ByteArray) = kept(name).apply { parentFile?.mkdirs(); writeBytes(b) }

    /** Rule 6: on opening the library (and every 6 h while sharing): the key, the list (applied), then the index. */
    fun refresh() {
        viewModelScope.launch {
            val r = withContext(io) {
                runCatching {
                    val keyJson = fetchOrKept("key") ?: return@runCatching Triple<BlockList?, LibraryIndex?, List<SharedCopies.Copy>>(null, null, emptyList())
                    val key = LibraryKey.read(keyJson, projectKey)
                    keep("key", keyJson)
                    val lastList = saved?.p2pListSequence ?: -1L
                    val list = fetchOrKept("blocklist.json")?.let { json ->
                        val sig = fetchOrKept("blocklist.json.sig") ?: throw BlockList.BadList("The block list has no signature")
                        BlockList.read(json, sig, key, lastList).also {
                            keep("blocklist.json", json); keep("blocklist.json.sig", sig)
                            saved?.p2pListSequence = it.sequence
                        }
                    }
                    val removed = list?.let { copies.apply(it) }.orEmpty()
                    val index = fetchOrKept("index.json")?.let { json ->
                        val sig = fetchOrKept("index.json.sig") ?: throw LibraryIndex.BadIndex("The index has no signature")
                        LibraryIndex.read(json, sig, key, saved?.p2pIndexSequence ?: -1L).also {
                            keep("index.json", json); keep("index.json.sig", sig)
                            saved?.p2pIndexSequence = it.sequence
                        }
                    }
                    Triple(list, index, removed)
                }
            }
            r.onFailure { AppLog.line("p2p: ${it.message}") }
            val kept = withContext(io) { copies.all() }
            _state.update { st ->
                val (list, index, removed) = r.getOrNull() ?: Triple(null, null, emptyList())
                st.copy(list = list ?: st.list, index = index ?: st.index, problem = r.exceptionOrNull()?.message,
                    withdrawn = st.withdrawn + removed.map { it.sha256 }, copies = kept)
            }
        }
    }

    fun setQuery(q: String) = _state.update { it.copy(query = q) }
    fun setSharing(on: Boolean) { saved?.p2pSharing = on; _state.update { it.copy(sharing = on) } }
    fun setUploadCap(mb: Int) { saved?.p2pUploadCapMb = mb; _state.update { it.copy(uploadCapMb = mb) } }
    /** The phone's network and charger, as the screen reads them (rule 3, P2P6). */
    fun setConditions(unmetered: Boolean, charging: Boolean) = _state.update { it.copy(unmetered = unmetered, charging = charging) }
    fun withdrawnSeen() = _state.update { it.copy(withdrawn = emptyList()) }

    /** Rule 7, «Denunciar»: a model (by its id) or a file (by its SHA-256), and the reason. */
    fun report(model: String?, sha256: String?, reason: String) {
        val s = server ?: return
        viewModelScope.launch {
            val r = withContext(io) { runCatching { s.report(model, sha256, reason) } }
            _state.update { it.copy(reported = r.getOrNull()?.let { model ?: sha256 }, reportFailed = r.exceptionOrNull()?.message) }
        }
    }

    fun reportSeen() = _state.update { it.copy(reported = null, reportFailed = null) }
}
