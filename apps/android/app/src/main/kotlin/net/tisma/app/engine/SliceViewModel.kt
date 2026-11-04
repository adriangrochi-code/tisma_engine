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
    /** The open model's triangles, standing on the bed, for the 3D view; and the bed's size (x, y). */
    val mesh: MeshFile? = null,
    val bed: FloatArray = floatArrayOf(220f, 220f),
    /** The printer card: the printer, nozzle and filament the model is sliced for. */
    val profile: EngineRunner.ProfileChoice? = null,
    /** The settings panel (Print, Material), and the profile's values under it, by `layers.height` keys. */
    val layout: SettingsLayout? = null,
    val profileValues: Map<String, kotlinx.serialization.json.JsonElement> = emptyMap(),
    val view: PanelVisibility.View = PanelVisibility.View.BASIC,
    val custom: Set<String> = emptySet(),
) {
    /** A setting's value now: the person's change, the plate's chips, the profile, or the engine's default. */
    fun value(key: String): kotlinx.serialization.json.JsonElement? = when (key) {
        "layers.height" -> kotlinx.serialization.json.JsonPrimitive(settings.layerHeight)
        "support.enabled" -> kotlinx.serialization.json.JsonPrimitive(settings.supports)
        else -> settings.changes[key] ?: profileValues[key]
            ?: layout?.rows?.get(key)?.let { SettingsLayout.toJson(it.editor, it.value) ?: kotlinx.serialization.json.JsonPrimitive(it.value) }
    }

    /** The setting differs from the profile (or from the engine's default when there is no profile value). */
    fun changed(key: String): Boolean = when (key) {
        "layers.height", "support.enabled" -> {
            val base = profileValues[key] ?: layout?.rows?.get(key)?.let { SettingsLayout.toJson(it.editor, it.value) }
            base != null && SettingsLayout.show(base) != SettingsLayout.show(value(key))
        }
        else -> key in settings.changes
    }
}

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
    /** Reads the settings panel (from the app's assets, in the phone's language); null when it is not there. */
    private val loadLayout: () -> SettingsLayout? = { null },
) : ViewModel() {
    private val _state = MutableStateFlow(SliceState(engineAvailable = runner.available,
        profile = saved?.profilePrinter?.let { EngineRunner.ProfileChoice(it, saved.profileNozzle, saved.profileFilament) },
        settings = savedSettings(),
        view = saved?.panelView?.let { v -> PanelVisibility.View.entries.firstOrNull { it.name.equals(v, true) } }
            ?: PanelVisibility.View.BASIC,
        custom = saved?.customVisible ?: emptySet()))

    private fun savedSettings(): EngineRunner.SimpleSettings {
        val o = saved?.panelChanges?.let { runCatching { kotlinx.serialization.json.Json.parseToJsonElement(it) as? kotlinx.serialization.json.JsonObject }.getOrNull() }
            ?: return EngineRunner.SimpleSettings()
        val h = (o["layers.height"] as? kotlinx.serialization.json.JsonPrimitive)?.content?.toDoubleOrNull()
        val sup = (o["support.enabled"] as? kotlinx.serialization.json.JsonPrimitive)?.content?.toBooleanStrictOrNull()
        return EngineRunner.SimpleSettings(h ?: 0.20, sup ?: false, o - setOf("layers.height", "support.enabled"))
    }

    private fun saveSettings(s: EngineRunner.SimpleSettings) {
        saved?.panelChanges = kotlinx.serialization.json.JsonObject(s.changes + mapOf(
            "layers.height" to kotlinx.serialization.json.JsonPrimitive(s.layerHeight),
            "support.enabled" to kotlinx.serialization.json.JsonPrimitive(s.supports))).toString()
    }

    init {
        viewModelScope.launch {
            val c = withContext(io) { runCatching { loadCatalog() }.getOrNull() }
            _state.update { it.copy(catalog = c) }
        }
        viewModelScope.launch {
            val l = withContext(io) { runCatching { loadLayout() }.onFailure { AppLog.line("panel: cannot read the layout ${it.message}") }.getOrNull() }
            _state.update { it.copy(layout = l) }
        }
        loadProfileValues()
    }

    /** The profile's values for the panel (`tisma-engine profile`), when the engine and a printer are there. */
    private fun loadProfileValues() {
        val choice = _state.value.profile
        if (choice == null || !runner.available) { _state.update { it.copy(profileValues = emptyMap()) }; return }
        viewModelScope.launch {
            val v = withContext(io) { runCatching { runner.profile(choice) }.getOrNull() }
            if (_state.value.profile == choice) _state.update { it.copy(profileValues = v?.let { SettingsLayout.flatten(it) } ?: emptyMap()) }
        }
    }

    /** Sets a setting from the panel, or puts it back to the profile's value with null. */
    fun setValue(key: String, value: kotlinx.serialization.json.JsonElement?) {
        val s = _state.value.settings
        val base = _state.value.profileValues[key] ?: _state.value.layout?.rows?.get(key)?.let { SettingsLayout.toJson(it.editor, it.value) }
        val n = when (key) {
            "layers.height" -> s.copy(layerHeight = ((value ?: base) as? kotlinx.serialization.json.JsonPrimitive)?.content?.toDoubleOrNull() ?: 0.20)
            "support.enabled" -> s.copy(supports = ((value ?: base) as? kotlinx.serialization.json.JsonPrimitive)?.content?.toBooleanStrictOrNull() ?: false)
            else -> s.copy(changes = if (value == null) s.changes - key else s.changes + (key to value))
        }
        setSettings(n)
    }

    /** Puts every setting of the panel back to the profile. */
    fun resetAll() {
        setSettings(EngineRunner.SimpleSettings())
        setValue("layers.height", null)
        setValue("support.enabled", null)
    }

    fun setView(v: PanelVisibility.View) {
        saved?.panelView = v.name.lowercase()
        _state.update { it.copy(view = v) }
    }

    /** Custom view: shows or hides a setting (settings_panel.md, «Elegir ajustes…»). */
    fun toggleCustom(key: String) {
        val c = _state.value.custom.let { if (key in it) it - key else it + key }
        saved?.customVisible = c
        _state.update { it.copy(custom = c) }
    }

    /** The bed of the chosen printer (catalog), or 220 × 220 mm. */
    private fun bedSize(): FloatArray {
        val p = _state.value.catalog?.printer(_state.value.profile?.printer)
        val v = p?.buildVolume
        return if (v != null && v.size >= 2 && v[0] > 0 && v[1] > 0) floatArrayOf(v[0].toFloat(), v[1].toFloat()) else floatArrayOf(220f, 220f)
    }

    /** Reads the model for the 3D view; a model it cannot read is still sliced by the engine. */
    private fun loadMesh(f: File) = viewModelScope.launch {
        val bed = bedSize()
        val mesh = withContext(io) { runCatching { MeshFile.read(f).onBed(bed[0] / 2, bed[1] / 2) }.getOrNull() }
        _state.update { it.copy(mesh = mesh, bed = bed) }
    }

    /** Picks the printer, nozzle and filament (the printer card). */
    fun setProfile(choice: EngineRunner.ProfileChoice?) {
        saved?.profilePrinter = choice?.printer
        choice?.let { saved?.profileNozzle = it.nozzle }
        saved?.profileFilament = choice?.filament
        _state.update { it.copy(profile = choice, result = null) }
        model?.let { loadMesh(it) }
        loadProfileValues()
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
        loadMesh(copied)
        _state.update {
            it.copy(modelName = name, isProject = copied.extension.equals("3mf", true), result = null, error = null, progress = 0)
        }
    }

    fun setSettings(s: EngineRunner.SimpleSettings) {
        saveSettings(s)
        _state.update { it.copy(settings = s, result = null) }
    }

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
    enum class Study { STRUCTURE, LIGHTEST, AERO }

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
                        Study.LIGHTEST -> runner.lightestInfill(m, objectIndex, quality, onProgress) { cancel.get() }
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
