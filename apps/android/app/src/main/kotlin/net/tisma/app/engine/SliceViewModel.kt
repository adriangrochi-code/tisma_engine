package net.tisma.app.engine

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
import java.io.InputStream
import java.io.OutputStream
import java.util.concurrent.atomic.AtomicBoolean

data class SliceState(
    /** The engine is in this APK (it can be left out of a build). */
    val engineAvailable: Boolean,
    val modelName: String? = null,
    /** A 3MF project brings its own settings; a bare model uses the simple ones. */
    val isProject: Boolean = false,
    val settings: EngineRunner.SimpleSettings = EngineRunner.SimpleSettings(),
    val slicing: Boolean = false,
    val progress: Int = 0,
    val result: EngineRunner.Slice? = null,
    val error: EngineRunner.EngineException? = null,
    val notice: SliceNotice? = null,
)

sealed interface SliceNotice {
    data object Cancelled : SliceNotice
    data object CannotRead : SliceNotice
    data class Saved(val name: String) : SliceNotice
}

/** Slicing on the phone with the engine (mobile_app.md, rule 12; MOBILE.md, step 5). */
class SliceViewModel(
    private val runner: EngineRunner,
    private val workDir: File,
    private val io: CoroutineDispatcher = Dispatchers.IO,
) : ViewModel() {
    private val _state = MutableStateFlow(SliceState(engineAvailable = runner.available))
    val state: StateFlow<SliceState> = _state.asStateFlow()
    private val cancel = AtomicBoolean(false)
    private var model: File? = null

    fun dismissNotice() = _state.update { it.copy(notice = null) }

    /** Copies the chosen file next to the engine's work (the engine reads paths, not Android's content addresses). */
    fun open(name: String, input: () -> InputStream?) = viewModelScope.launch {
        val copied = withContext(io) {
            runCatching {
                File(workDir, "input").deleteRecursively()
                val dir = File(workDir, "input").apply { mkdirs() }
                val safe = name.substringAfterLast('/').replace(Regex("[^A-Za-z0-9._ -]"), "_").ifBlank { "model.stl" }
                val f = File(dir, safe)
                (input() ?: return@runCatching null).use { i -> f.outputStream().use { i.copyTo(it) } }
                f
            }.getOrNull()
        }
        if (copied == null) {
            _state.update { it.copy(notice = SliceNotice.CannotRead) }
            return@launch
        }
        model = copied
        _state.update {
            it.copy(modelName = name, isProject = copied.extension.equals("3mf", true), result = null, error = null, progress = 0)
        }
    }

    fun setSettings(s: EngineRunner.SimpleSettings) = _state.update { it.copy(settings = s, result = null) }

    fun slice() {
        val m = model ?: return
        if (state.value.slicing) return
        cancel.set(false)
        _state.update { it.copy(slicing = true, progress = 0, result = null, error = null) }
        val s = state.value
        viewModelScope.launch {
            try {
                val started = System.currentTimeMillis()
                val r = withContext(io) {
                    val onProgress = { p: Int -> _state.update { it.copy(progress = p) } }
                    if (s.isProject) runner.sliceProject(m, onProgress) { cancel.get() }
                    else runner.sliceModel(m, s.settings, onProgress) { cancel.get() }
                }
                AppLog.line("engine: sliced in ${System.currentTimeMillis() - started} ms")
                _state.update { it.copy(result = r, progress = 100) }
            } catch (e: EngineRunner.Cancelled) {
                _state.update { it.copy(notice = SliceNotice.Cancelled) }
            } catch (e: EngineRunner.EngineException) {
                AppLog.line("engine: failed (${e.exitCode}) ${e.error}")
                _state.update { it.copy(error = e) }
            } catch (e: java.io.IOException) {
                AppLog.line("engine: cannot run ${e.message}")
                _state.update { it.copy(error = EngineRunner.EngineException("error", e.message ?: "", "", -1)) }
            } finally {
                _state.update { it.copy(slicing = false) }
            }
        }
    }

    fun cancel() = cancel.set(true)

    fun saveGcode(name: String, open: () -> OutputStream?) = viewModelScope.launch {
        val g = state.value.result?.gcode ?: return@launch
        val ok = withContext(io) {
            runCatching { (open() ?: return@runCatching false).use { out -> g.inputStream().use { it.copyTo(out) } }; true }
                .getOrDefault(false)
        }
        _state.update { it.copy(notice = if (ok) SliceNotice.Saved(name) else SliceNotice.CannotRead) }
    }

    override fun onCleared() {
        cancel.set(true)
    }
}
