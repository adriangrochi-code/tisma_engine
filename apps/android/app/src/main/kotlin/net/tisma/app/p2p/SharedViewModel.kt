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

/** Why a model did not open on the plate (rule 8 octies), in the person's words on the screen. */
enum class OpenNotice { NOT_YET, BLOCKED, BUSY, SIGN_IN, DISCARDED, NOTHING_TO_OPEN, FAILED }

/** Rule 8 octies: a download's pieces as files, so it resumes after a cut and is checked whole at the end. */
class FilePieceStore(private val dir: File) : PieceStore {
    private fun f(n: Int) = File(dir, n.toString())
    override fun has(n: Int) = f(n).isFile
    override fun write(n: Int, bytes: ByteArray) {
        dir.mkdirs()
        val tmp = File(dir, "$n.part")
        tmp.writeBytes(bytes)
        if (!tmp.renameTo(f(n))) throw java.io.IOException("cannot keep piece $n")
    }
    override fun read(n: Int) = f(n).readBytes()
    override fun clear() { dir.deleteRecursively() }
    /** The pieces in order, into [out]; then the pieces go. */
    fun assemble(count: Int, out: File) {
        out.parentFile?.mkdirs()
        out.outputStream().use { o -> for (n in 0 until count) o.write(read(n)) }
        clear()
    }
}

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
    /** «Abrir en la placa» (rule 8 octies): the model being downloaded and how far (0–100). */
    val opening: String? = null,
    val openPercent: Int = 0,
    val openNotice: OpenNotice? = null,
    /** A downloaded, checked file for the plate: its name and where it is. */
    val toOpen: Pair<String, File>? = null,
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

    /**
     * «Abrir en la placa» (rule 8 octies, provisional): the model's project or model file from the seeder, checked
     * piece by piece against the signed index and whole, kept as a copy the phone may share (rule 8), then handed to
     * the plate through [SharedState.toOpen].
     */
    fun openOnPlate(m: LibraryIndex.Model) {
        if (_state.value.opening != null) return
        fun say(n: OpenNotice) = _state.update { it.copy(openNotice = n, opening = null) }
        val s = server
        val seeder = s?.seeder ?: return say(OpenNotice.FAILED)
        val file = LibraryOpen.fileToOpen(m) ?: return say(OpenNotice.NOTHING_TO_OPEN)
        val root = m.pieceRoots[file.sha256.lowercase()] ?: return say(OpenNotice.DISCARDED)
        _state.update { it.copy(opening = m.id, openPercent = 0, openNotice = null) }
        viewModelScope.launch {
            val r = withContext(io) {
                runCatching {
                    val parts = FilePieceStore(File(dir, "downloads/" + file.sha256.lowercase()))
                    val result = SeedDownload(seeder) { s.ticket() }.download(file, root, parts) { done, of ->
                        _state.update { it.copy(openPercent = if (of == 0) 100 else done * 100 / of) }
                    }
                    if (result is SeedDownload.Result.Done) {
                        val ext = file.path.substringAfterLast('.', "3mf").lowercase()
                        val whole = File(dir, "downloads/" + file.sha256.lowercase() + "." + ext)
                        parts.assemble(SeedClient.pieceCount(file.size), whole)
                        val copy = copies.add(whole, ext)
                        whole.delete()
                        result to copy.file
                    } else result to null
                }
            }
            r.onFailure { AppLog.line("p2p: open ${m.id} ${it.message}") }
            val (result, copy) = r.getOrNull() ?: (null to null)
            val notice = when {
                copy != null -> null
                r.exceptionOrNull()?.message == "No session" ||
                    (r.exceptionOrNull() as? net.tisma.cloud.CloudException)?.status == 401 -> OpenNotice.SIGN_IN
                result is SeedDownload.Result.NotYet -> OpenNotice.NOT_YET
                result is SeedDownload.Result.Blocked -> OpenNotice.BLOCKED
                result is SeedDownload.Result.Busy -> OpenNotice.BUSY
                result is SeedDownload.Result.Unauthorized -> OpenNotice.SIGN_IN
                result is SeedDownload.Result.BadList || result is SeedDownload.Result.Corrupt -> OpenNotice.DISCARDED
                else -> OpenNotice.FAILED
            }
            val kept = withContext(io) { copies.all() }
            _state.update {
                it.copy(opening = null, openNotice = notice, copies = kept,
                    toOpen = copy?.let { f -> LibraryOpen.name(file) to f })
            }
        }
    }

    /** The plate took the file. */
    fun opened() = _state.update { it.copy(toOpen = null) }

    fun openNoticeSeen() = _state.update { it.copy(openNotice = null) }
}
