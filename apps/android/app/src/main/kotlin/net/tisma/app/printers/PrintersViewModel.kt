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
)

/** What the camera box shows (printer_connections.md, «Cámara de la impresora», rule 1). */
sealed interface CameraView {
    data object Connecting : CameraView
    data object None : CameraView
    data class Failed(val reason: String) : CameraView
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

    private fun connection(c: PrinterConfig) = connector(c, store.secret(c.id))

    /** Tests a printer before it is saved; [onOk] gets the saved printer. */
    fun testAndSave(id: String?, name: String, kind: PrinterKind, address: String, secret: String?,
                    onOk: (PrinterConfig) -> Unit) = launch {
        val config = PrinterConfig(id ?: PrinterStore.newId(), name.trim().ifEmpty { address.trim() }, kind,
            PrinterConfig.normalizeUrl(address, kind))
        val stored = secret ?: id?.let { store.secret(it) }
        val text = withContext(io) { connector(config, stored).test() }
        store.save(config, secret)
        AppLog.line("printer: ${kind.wire} saved ($text)")
        _state.update { it.copy(printers = store.all(), notice = PrinterNotice.Tested(text)) }
        refreshFound()
        onOk(config)
    }

    fun remove(id: String) {
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
            setCamera(id, CameraView.None)
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
    fun upload(id: String, name: String, length: Long, start: Boolean, open: () -> InputStream?) {
        val p = state.value.printers.firstOrNull { it.id == id } ?: return
        _state.update { it.copy(uploading = true) }
        viewModelScope.launch {
            try {
                withContext(io) {
                    connection(p).upload(name, length, start) {
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
