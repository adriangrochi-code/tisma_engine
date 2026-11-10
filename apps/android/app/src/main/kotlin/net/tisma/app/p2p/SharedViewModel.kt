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
import java.time.Instant

/** The shared library's screen (p2p_library.md): the block list, what was withdrawn, and whether sharing may run. */
data class SharedState(
    val list: BlockList? = null,
    /** Why the last list was not used (no signature, a bad one, cannot be read). */
    val listProblem: String? = null,
    /** Rule 4: copies deleted because the list withdrew them, to tell the person. */
    val withdrawn: List<String> = emptyList(),
    val copies: List<SharedCopies.Copy> = emptyList(),
    val sharing: Boolean = false,
    val uploadCapMb: Int = 500,
    val unmetered: Boolean = false,
    val charging: Boolean = false,
) {
    val blockedBy: SharingPolicy.Why? get() = SharingPolicy.blocked(sharing, list, unmetered, charging)
}

/**
 * [fetch] gives the list and its signature (the server's addresses are not decided yet: null keeps the last one
 * kept on the phone, rule 6: «sin red, usa la última»). [publicKey] is Tisma's (BuildConfig.UPDATES_KEY); empty
 * until the owner's key exists, and then no list is ever accepted and sharing stays off.
 */
class SharedViewModel(
    private val dir: File,
    private val publicKey: ByteArray,
    private val fetch: (() -> Pair<ByteArray, ByteArray>?)? = null,
    private val saved: net.tisma.app.data.AppSettings? = null,
    private val io: CoroutineDispatcher = Dispatchers.IO,
) : ViewModel() {
    private val copies = SharedCopies(File(dir, "files"))
    private val _state = MutableStateFlow(SharedState(sharing = saved?.p2pSharing ?: false, uploadCapMb = saved?.p2pUploadCapMb ?: 500))
    val state: StateFlow<SharedState> = _state.asStateFlow()

    /** Rule 6: on opening the library (and every 6 hours while sharing), the newest signed list; then applied. */
    fun refresh(now: Instant = Instant.now()) {
        viewModelScope.launch {
            val r = withContext(io) {
                runCatching {
                    fetch?.invoke()?.let { (json, sig) ->
                        BlockList.read(json, sig, publicKey, now) // checked before it is kept
                        File(dir, "blocklist.json").apply { parentFile?.mkdirs(); writeBytes(json) }
                        File(dir, "blocklist.json.sig").writeBytes(sig)
                    }
                    val json = File(dir, "blocklist.json"); val sig = File(dir, "blocklist.json.sig")
                    if (!json.isFile || !sig.isFile) return@runCatching null
                    BlockList.read(json.readBytes(), sig.readBytes(), publicKey, now)
                }
            }
            r.onFailure { AppLog.line("p2p: block list ${it.message}") }
            val list = r.getOrNull()
            val removed = if (list != null) withContext(io) { copies.apply(list) } else emptyList()
            val kept = withContext(io) { copies.all() }
            _state.update {
                it.copy(list = list ?: it.list, listProblem = r.exceptionOrNull()?.message,
                    withdrawn = it.withdrawn + removed.map { c -> c.sha256 }, copies = kept)
            }
        }
    }

    fun setSharing(on: Boolean) { saved?.p2pSharing = on; _state.update { it.copy(sharing = on) } }

    fun setUploadCap(mb: Int) { saved?.p2pUploadCapMb = mb; _state.update { it.copy(uploadCapMb = mb) } }

    /** The phone's network and charger, as the screen reads them (rule 3, P2P6). */
    fun setConditions(unmetered: Boolean, charging: Boolean) = _state.update { it.copy(unmetered = unmetered, charging = charging) }

    fun withdrawnSeen() = _state.update { it.copy(withdrawn = emptyList()) }
}
