package net.tisma.app.printers

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import net.tisma.app.data.AppLog
import net.tisma.printers.PrinterConfig
import net.tisma.printers.PrinterConnection
import net.tisma.printers.PrinterException
import net.tisma.printers.PrinterKind
import net.tisma.printers.PrinterState
import net.tisma.printers.PrinterStatus
import net.tisma.printers.connect
import java.io.InputStream

data class PrintersState(
    val printers: List<PrinterConfig> = emptyList(),
    /** The last state read of each printer, by id; OFFLINE with the reason when it did not answer. */
    val status: Map<String, PrinterStatus> = emptyMap(),
    val busy: Boolean = false,
    val uploading: Boolean = false,
    val notice: PrinterNotice? = null,
    /** The camera of each printer on screen, by id. */
    val cameras: Map<String, CameraView> = emptyMap(),
    /** Printers found on the network and not saved yet. */
    val found: List<net.tisma.printers.FoundPrinter> = emptyList(),
    /** A Bambu Lab printer waiting for the person to confirm its serial and certificate (BL3). */
    val confirm: PendingCertificate? = null,
)

/** What the person confirms the first time a Bambu Lab printer is added. */
data class PendingCertificate(val config: PrinterConfig, val secret: String?, val serial: String, val fingerprint: String,
                              val text: String)

/** What the camera box shows (printer_connections.md, «Cámara de la impresora», rule 1). */
sealed interface CameraView {
    data object Connecting : CameraView
    data object None : CameraView
    data class Failed(val reason: String) : CameraView
    /** A Bambu Lab camera that failed, with what to check (QA BH36: say clearly why it is not seen). */
    data class BambuFailed(val kind: PrinterException.Kind, val detail: String) : CameraView
    /** The last image, as JPEG bytes. */
    class Image(val jpeg: ByteArray) : CameraView
}

sealed interface PrinterNotice {
    data class Failed(val error: PrinterException) : PrinterNotice
    /** The connection test passed: the program and its version. */
    data class Tested(val text: String) : PrinterNotice
    data class Uploaded(val name: String, val started: Boolean) : PrinterNotice
    data object CannotRead : PrinterNotice
}

/** The printers on the network (printer_connections.md; MOBILE.md, «Impresoras», «Impresión», «Controles»). */
class PrintersViewModel(
    private val store: PrinterStore,
    private val io: CoroutineDispatcher = Dispatchers.IO,
    private val pollMs: Long = POLL_MS,
    private val connector: (PrinterConfig, String?) -> PrinterConnection = { c, s -> c.connect(s) },
    /** Called when a printer is seen printing: the app starts following it in the background. */
    private val onActivePrint: () -> Unit = {},
    /** Reads a Bambu Lab printer's test text, serial and certificate fingerprint (replaceable in tests). */
    private val probeBambu: (url: String, code: String) -> Triple<String, String, String> = { url, code ->
        val b = net.tisma.printers.BambuConnection.of(url, code, null)
        try {
            Triple(b.test(), b.seenSerial.orEmpty(), b.seenFingerprint.orEmpty())
        } finally {
            b.close()
        }
    },
) : ViewModel() {
    private val _state = MutableStateFlow(PrintersState(printers = store.all()))
    val state: StateFlow<PrintersState> = _state.asStateFlow()

    private val foundList = net.tisma.printers.FoundPrinters()

    /** From the network search (any thread). */
    fun onFound(p: net.tisma.printers.FoundPrinter) {
        foundList.seen(p, System.currentTimeMillis())
        refreshFound()
    }

    fun onLost(host: String, port: Int) {
        foundList.lost(host, port)
        refreshFound()
    }

    fun refreshFound() = _state.update {
        it.copy(found = foundList.current(System.currentTimeMillis(), it.printers.map { p -> p.url }))
    }

    fun dismissNotice() = _state.update { it.copy(notice = null) }

    // Connections are kept while the config is the same: Bambu Lab's MQTT stays open between reads.
    private val open = mutableMapOf<String, Pair<PrinterConfig, PrinterConnection>>()

    @Synchronized
    private fun connection(c: PrinterConfig): PrinterConnection {
        open[c.id]?.let { (cfg, conn) -> if (cfg == c) return conn else conn.close() }
        return connector(c, store.secret(c.id)).also { open[c.id] = c to it }
    }

    @Synchronized
    private fun forget(id: String) {
        open.remove(id)?.second?.close()
    }

    override fun onCleared() {
        synchronized(this) { open.values.forEach { it.second.close() }; open.clear() }
    }

    /** Tests a printer before it is saved; [onOk] gets the saved printer. */
    fun testAndSave(id: String?, name: String, kind: PrinterKind, address: String, secret: String?, user: String = "",
                    onOk: (PrinterConfig) -> Unit) = launch {
        val config = PrinterConfig(id ?: PrinterStore.newId(), name.trim().ifEmpty { address.trim() }, kind,
            PrinterConfig.normalizeUrl(address, kind), user.trim())
        val stored = secret ?: id?.let { store.secret(it) }
        val kept = id?.let { i -> store.all().firstOrNull { it.id == i } }
        if (kind == PrinterKind.BAMBU && (kept?.certSha256.isNullOrBlank() || kept?.url != config.url)) {
            // First time (or a new address): read the serial and the certificate for the person to confirm.
            val (text, serial, fingerprint) = withContext(io) { probeBambu(config.url, stored.orEmpty()) }
            _state.update { it.copy(confirm = PendingCertificate(config, secret, serial, fingerprint, text)) }
            return@launch
        }
        val toSave = if (kind == PrinterKind.BAMBU && kept != null) config.copy(serial = kept.serial, certSha256 = kept.certSha256) else config
        val text = withContext(io) { connector(toSave, stored).let { c -> try { c.test() } finally { c.close() } } }
        forget(toSave.id)
        store.save(toSave, secret)
        AppLog.line("printer: ${kind.wire} saved ($text)")
        _state.update { it.copy(printers = store.all(), notice = PrinterNotice.Tested(text)) }
        refreshFound()
        onOk(config)
    }

    /** The person confirmed the printer's serial and certificate: saved with them (rule 4). */
    fun confirmCertificate(onOk: (PrinterConfig) -> Unit) {
        val p = state.value.confirm ?: return
        val saved = store.save(p.config.copy(serial = p.serial, certSha256 = p.fingerprint), p.secret)
        AppLog.line("printer: bambu saved, certificate confirmed")
        _state.update { it.copy(confirm = null, printers = store.all(), notice = PrinterNotice.Tested(p.text)) }
        refreshFound()
        onOk(saved)
    }

    fun rejectCertificate() = _state.update { it.copy(confirm = null) }

    fun remove(id: String) {
        forget(id)
        store.remove(id)
        _state.update { s -> s.copy(printers = store.all(), status = s.status - id) }
    }

    /** Reads every printer's state every [pollMs] while the list is on screen. */
    suspend fun followAll() {
        while (currentCoroutineContext().isActive) {
            state.value.printers.forEach { p -> read(p) }
            delay(pollMs)
        }
    }

    /** Reads one printer's state every [pollMs] while its screen is open. */
    suspend fun follow(id: String) {
        while (currentCoroutineContext().isActive) {
            state.value.printers.firstOrNull { it.id == id }?.let { read(it) } ?: return
            delay(pollMs)
        }
    }

    /** Shows a printer's camera while its screen is open: one image every [cameraMs]. */
    suspend fun followCamera(id: String, cameraMs: Long = 1_000) {
        val p = state.value.printers.firstOrNull { it.id == id } ?: return
        setCamera(id, CameraView.Connecting)
        val camera = try {
            withContext(io) { connection(p).camera() }
        } catch (e: PrinterException) {
            setCamera(id, CameraView.Failed(e.detail))
            return
        }
        val url = camera?.snapshotUrl?.takeIf { it.isNotBlank() }
        if (url == null) {
            // A camera that sends its own images (Bambu Lab A1 and P1, printer_connections.md, Bambu Lab rule 6).
            if (p.kind == net.tisma.printers.PrinterKind.BAMBU) followFrames(id, p) else setCamera(id, CameraView.None)
            return
        }
        try {
            while (currentCoroutineContext().isActive) {
                val view = try {
                    CameraView.Image(withContext(io) { net.tisma.printers.Snapshot.fetch(url) })
                } catch (e: PrinterException) {
                    CameraView.Failed(e.detail)
                }
                setCamera(id, view)
                delay(cameraMs)
            }
        } finally {
            // Images live only while they are shown.
            _state.update { it.copy(cameras = it.cameras - id) }
        }
    }

    private suspend fun followFrames(id: String, p: PrinterConfig) {
        try {
            withContext(io) {
                connection(p).cameraFrames()?.use { frames ->
                    while (currentCoroutineContext().isActive) {
                        val jpeg = frames.next()
                        setCamera(id, CameraView.Image(jpeg))
                    }
                } ?: setCamera(id, CameraView.None)
            }
        } catch (e: PrinterException) {
            AppLog.line("printer: camera ${e.kind} ${e.detail}")
            setCamera(id, CameraView.BambuFailed(e.kind, e.detail))
        } catch (e: kotlinx.coroutines.CancellationException) {
            // Images live only while they are shown.
            _state.update { it.copy(cameras = it.cameras - id) }
            throw e
        }
    }

    private fun setCamera(id: String, v: CameraView) = _state.update { it.copy(cameras = it.cameras + (id to v)) }

    private suspend fun read(p: PrinterConfig) {
        val s = try {
            withContext(io) { connection(p).status() }
        } catch (e: PrinterException) {
            PrinterStatus(if (e.kind == PrinterException.Kind.AUTH) PrinterState.ERROR else PrinterState.OFFLINE,
                message = e.detail)
        }
        _state.update { it.copy(status = it.status + (p.id to s)) }
        if (net.tisma.printers.PrintEvents.isActive(s)) onActivePrint()
    }

    fun setTemperature(id: String, heater: net.tisma.printers.Heater, target: Double) =
        command(id) { it.setTemperature(heater, target) }
    fun home(id: String) = command(id) { it.home() }
    fun jog(id: String, axis: net.tisma.printers.Axis, distance: Double) =
        command(id) { it.jog(axis, distance, if (axis == net.tisma.printers.Axis.Z) 10.0 else 50.0) }
    fun extrude(id: String, mm: Double) = command(id) { it.extrude(mm, 5.0) }

    fun pause(id: String) = command(id) { it.pause() }
    fun resume(id: String) = command(id) { it.resume() }
    fun cancel(id: String) = command(id) { it.cancel() }

    /** Uploads a G-code to a printer and starts it if [start] (MA7). */
    fun upload(id: String, name: String, length: Long, start: Boolean, open: () -> InputStream?) =
        upload(id, name, length, start, net.tisma.printers.PrintOptions(), open)

    /** The same with the Bambu Lab print options (gcode_macros.md, rule 12); other printers ignore them. */
    fun upload(id: String, name: String, length: Long, start: Boolean, options: net.tisma.printers.PrintOptions,
               open: () -> InputStream?) {
        val p = state.value.printers.firstOrNull { it.id == id } ?: return
        _state.update { it.copy(uploading = true) }
        viewModelScope.launch {
            try {
                withContext(io) {
                    connection(p).upload(name, length, start, options) {
                        open() ?: throw java.io.IOException("Cannot read $name")
                    }
                }
                AppLog.line("printer: uploaded a G-code (start=$start)")
                _state.update { it.copy(notice = PrinterNotice.Uploaded(name, start)) }
                read(p)
            } catch (e: PrinterException) {
                fail(e)
            } catch (e: java.io.IOException) {
                _state.update { it.copy(notice = PrinterNotice.CannotRead) }
            } finally {
                _state.update { it.copy(uploading = false) }
            }
        }
    }

    private fun command(id: String, block: (PrinterConnection) -> Unit) {
        val p = state.value.printers.firstOrNull { it.id == id } ?: return
        launch {
            withContext(io) { block(connection(p)) }
            read(p)
        }
    }

    private fun launch(block: suspend () -> Unit) = viewModelScope.launch {
        _state.update { it.copy(busy = true) }
        try {
            block()
        } catch (e: PrinterException) {
            fail(e)
        } finally {
            _state.update { it.copy(busy = false) }
        }
    }

    private fun fail(e: PrinterException) {
        AppLog.line("printer: ${e.kind.name.lowercase()} ${e.detail}")
        _state.update { it.copy(notice = PrinterNotice.Failed(e)) }
    }

    companion object {
        const val POLL_MS = 2_000L
    }
}
