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
    val catalog: Catalog? = null,
    /** An Engineering run on the phone: running, and its summary when done (mobile_app.md, rule 13). */
    val analyzing: Boolean = false,
    val analysis: List<Pair<String, String>>? = null,
    /** MA9: the model needs more memory than the phone has free (bytes needed, bytes free). */
    val tooBig: Pair<Long, Long>? = null,
    /** The printer card: the printer, nozzle and filament the model is sliced for. */
    val profile: EngineRunner.ProfileChoice? = null,
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
    /** Reads the catalog (from the app's assets); null when it is not there. */
    private val loadCatalog: () -> Catalog? = { null },
    /** Where the printer card's choice is kept. */
    private val saved: net.tisma.app.data.AppSettings? = null,
    /** Free memory for the engine, in bytes (the phone's available memory). */
    private val freeMemory: () -> Long = { Long.MAX_VALUE },
) : ViewModel() {
    private val _state = MutableStateFlow(SliceState(engineAvailable = runner.available,
        profile = saved?.profilePrinter?.let { EngineRunner.ProfileChoice(it, saved.profileNozzle, saved.profileFilament) }))

    init {
        viewModelScope.launch {
            val c = withContext(io) { runCatching { loadCatalog() }.getOrNull() }
            _state.update { it.copy(catalog = c) }
        }
    }

    /** Picks the printer, nozzle and filament (the printer card). */
    fun setProfile(choice: EngineRunner.ProfileChoice?) {
        saved?.profilePrinter = choice?.printer
        choice?.let { saved?.profileNozzle = it.nozzle }
        saved?.profileFilament = choice?.filament
        _state.update { it.copy(profile = choice, result = null) }
    }
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

    /** Slices even when the estimate says the memory may not be enough (the person chose it). */
    fun sliceAnyway() {
        _state.update { it.copy(tooBig = null) }
        slice(checkMemory = false)
    }

    fun dismissTooBig() = _state.update { it.copy(tooBig = null) }

    fun slice(checkMemory: Boolean = true) {
        val m = model ?: return
        if (state.value.slicing) return
        if (checkMemory) {
            val need = MemoryEstimate.needed(MemoryEstimate.triangles(m))
            val free = freeMemory()
            if (need > free) {
                AppLog.line("engine: model needs ~${need shr 20} MB, ${free shr 20} MB free")
                _state.update { it.copy(tooBig = need to free) }
                return
            }
        }
        cancel.set(false)
        _state.update { it.copy(slicing = true, progress = 0, result = null, error = null) }
        val s = state.value
        viewModelScope.launch {
            try {
                val started = System.currentTimeMillis()
                val r = withContext(io) {
                    val onProgress = { p: Int -> _state.update { it.copy(progress = p) } }
                    if (s.isProject) runner.sliceProject(m, onProgress) { cancel.get() }
                    else runner.sliceModel(m, s.settings, onProgress, { cancel.get() }, s.profile)
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

    /** What Engineering runs on the phone. */
    enum class Study { STRUCTURE, AERO }

    /** Runs an analysis of the open 3MF project with the engine (its conditions come from the desktop). */
    fun analyze(study: Study, objectIndex: Int, quality: String?, speed: Double = 10.0, direction: String = "x") {
        val m = model ?: return
        if (state.value.slicing || state.value.analyzing) return
        cancel.set(false)
        _state.update { it.copy(analyzing = true, progress = 0, analysis = null, error = null) }
        viewModelScope.launch {
            try {
                val r = withContext(io) {
                    val onProgress = { p: Int -> _state.update { it.copy(progress = p) } }
                    when (study) {
                        Study.STRUCTURE -> runner.analyze(m, objectIndex, quality, onProgress) { cancel.get() }
                        Study.AERO -> runner.aero(m, objectIndex, quality ?: "fast", speed, direction, onProgress) { cancel.get() }
                    }
                }
                val lines = r.summary.mapNotNull { (k, v) ->
                    (v as? kotlinx.serialization.json.JsonPrimitive)?.let { k to it.content }
                }.filter { (k, _) -> k !in setOf("code", "error", "error_es") }
                AppLog.line("engine: ${study.name.lowercase()} done")
                _state.update { it.copy(analysis = lines) }
            } catch (e: EngineRunner.Cancelled) {
                _state.update { it.copy(notice = SliceNotice.Cancelled) }
            } catch (e: EngineRunner.EngineException) {
                AppLog.line("engine: ${study.name.lowercase()} failed (${e.exitCode}) ${e.error}")
                _state.update { it.copy(error = e) }
            } catch (e: java.io.IOException) {
                _state.update { it.copy(error = EngineRunner.EngineException("error", e.message ?: "", "", -1)) }
            } finally {
                _state.update { it.copy(analyzing = false) }
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
