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
    /** Rule 3 ter bis: the index is larger than this version takes. */
    val tooLarge: Boolean = false,
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

    /** The kept pieces in [dir]; the sequences and the server in [saved] (none kept without it, as before). */
    private val store = object : LibraryStore {
        private fun kept(name: String) = File(dir, name)
        override fun read(name: String) = kept(name).takeIf { it.isFile }?.readBytes()
        override fun write(name: String, bytes: ByteArray) { kept(name).apply { parentFile?.mkdirs(); writeBytes(bytes) } }
        override fun delete(name: String) { kept(name).delete() }
        override var listSequence: Long
            get() = saved?.p2pListSequence ?: -1L
            set(v) { saved?.p2pListSequence = v }
        override var indexSequence: Long
            get() = saved?.p2pIndexSequence ?: -1L
            set(v) { saved?.p2pIndexSequence = v }
        override var origin: String?
            get() = saved?.p2pOrigin
            set(v) { if (v != null) saved?.p2pOrigin = v }
    }

    /** Rule 6: on opening the library (and every 6 h while sharing): the key, the list (applied), then the index. */
    fun refresh() {
        viewModelScope.launch {
            val r = withContext(io) {
                runCatching {
                    val fetched = LibraryFetch(projectKey, server, store) { AppLog.line(it) }.refresh()
                    val list = fetched.list
                    val index = fetched.index
                    val removed = list?.let { copies.apply(it) }.orEmpty()
                    Triple(list, index, removed)
                }
            }
            r.onFailure { AppLog.line("p2p: ${it.message}") }
            val kept = withContext(io) { copies.all() }
            _state.update { st ->
                val (list, index, removed) = r.getOrNull() ?: Triple(null, null, emptyList())
                st.copy(list = list ?: st.list, index = index ?: st.index, problem = r.exceptionOrNull()?.let { e -> e.message ?: e::class.simpleName ?: "?" },
                    tooLarge = r.exceptionOrNull() is LibraryIndex.TooLarge,
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
            _state.update { it.copy(reported = r.getOrNull()?.let { model ?: sha256 }, reportFailed = r.exceptionOrNull()?.let { e -> e.message?.takeIf { it.isNotBlank() } ?: e::class.simpleName ?: "?" }) }
        }
    }

    fun reportSeen() = _state.update { it.copy(reported = null, reportFailed = null) }
}
