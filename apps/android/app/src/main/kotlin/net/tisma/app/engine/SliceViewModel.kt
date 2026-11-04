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
    /** The profile pack for the wizard: its index, a download running, its progress, and what failed. */
    val packAvailable: Boolean = false,
    val packIndex: ProfilePack.Index? = null,
    val packFilaments: List<ProfilePack.Filament> = emptyList(),
    /** The pack printer's bed: its 3D model placed on the bed, and its picture file (rule 14). */
    val bedModel: MeshFile? = null,
    val bedTexture: File? = null,
    val packBusy: Boolean = false,
    val packProgress: Int = 0,
    val packError: String? = null,
    /** Engineering on a bare model: the marked faces, the material and the safety asked (mobile_app.md, rule 13). */
    val case: EngineeringCase = EngineeringCase(),
    /** The last analysis' safety map on the part, and whether the 3D view shows it instead of the part. */
    val stressMap: StressMap? = null,
    val showStress: Boolean = false,
    /** What a tap on the 3D view marks now, or null when it only turns the view. */
    val marking: EngineeringCase.Kind? = null,
    /** The lightest infill found that holds, in %, to apply to the slice (mobile_app.md, rule 13). */
    val lightest: Int? = null,
    /** A G-code that was opened or shared, to show in the preview (QA AND5). */
    val gcodeToPreview: String? = null,
    val custom: Set<String> = emptySet(),
) {
    /** A setting's value now: the person's change, the plate's chips, the profile, or the engine's default. */
    fun value(key: String): kotlinx.serialization.json.JsonElement? = when (key) {
        "layers.height" -> kotlinx.serialization.json.JsonPrimitive(settings.layerHeight)
        "support.enabled" -> kotlinx.serialization.json.JsonPrimitive(settings.supports)
        else -> settings.changes[key] ?: profileValues[key]
            // A row without its own value takes another's (the first layer's temperature is the other layers').
            ?: layout?.rows?.get(key)?.fallback?.takeIf { it != key }?.let { value(it) }
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
    /** The free profile pack (profile_pack.md), or null when there is no engine to install it. */
    private val pack: ProfilePack? = null,
    /** The pack's address; null for the published one (tests give their server). */
    private val packUrl: String? = null,
) : ViewModel() {
    private val _state = MutableStateFlow(SliceState(engineAvailable = runner.available,
        profile = saved?.profilePrinter?.let {
            EngineRunner.ProfileChoice(it, saved.profileNozzle, saved.profileFilament, saved.profilePack, saved.profileName)
        },
        settings = savedSettings(),
        view = saved?.panelView?.let { v -> PanelVisibility.View.entries.firstOrNull { it.name.equals(v, true) } }
            ?: PanelVisibility.View.BASIC,
        custom = saved?.customVisible ?: emptySet(), packAvailable = pack != null))

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
            if (_state.value.profile == choice) {
                _state.update { it.copy(profileValues = v?.let { SettingsLayout.flatten(it) } ?: emptyMap()) }
                // The bed may have changed size: the model goes back to its centre, and the pack's bed is drawn.
                val bed = bedSize()
                _state.update { it.copy(bed = bed) }
                model?.let { loadMesh(it) }
                loadBedAssets(choice, bed)
            }
        }
    }

    /**
     * The pack printer's bed model and picture (profile_pack.md, rule 14). The
     * STL keeps its height (its top is the bed's surface); its plan is centred
     * on the bed, since the pack's files are centred on their origin (asked to
     * the spec team).
     */
    private fun loadBedAssets(choice: EngineRunner.ProfileChoice?, bed: FloatArray) {
        val p = pack
        if (choice == null || !choice.pack || p == null) { _state.update { it.copy(bedModel = null, bedTexture = null) }; return }
        viewModelScope.launch {
            val r = withContext(io) {
                runCatching {
                    val file = choice.printer.substringBefore('#')
                    val m = p.index()?.vendors?.flatMap { it.models }?.firstOrNull { it.file == file } ?: return@runCatching null
                    val stl = m.bedModel.takeIf { it.isNotBlank() }?.let { File(p.dataDir, "pack/$it") }?.takeIf { it.isFile }
                    val mesh = stl?.let { MeshFile.read(it) }?.let { bedOnBed(it, bed) }
                    val tex = m.bedTexture.takeIf { it.isNotBlank() }?.let { File(p.dataDir, "pack/$it") }?.takeIf { it.isFile }
                    mesh to tex
                }.onFailure { AppLog.line("bed: ${it.message}") }.getOrNull()
            }
            if (_state.value.profile == choice) _state.update { it.copy(bedModel = r?.first, bedTexture = r?.second) }
        }
    }

    private fun bedOnBed(m: MeshFile, bed: FloatArray): MeshFile {
        val b = m.bounds
        val dx = bed[0] / 2 - (b[0] + b[3]) / 2
        val dy = bed[1] / 2 - (b[1] + b[4]) / 2
        // A little under the grid, so the lines stay seen on its top.
        val dz = -0.05f
        val p = m.positions.copyOf()
        var i = 0
        while (i < p.size) { p[i] += dx; p[i + 1] += dy; p[i + 2] += dz; i += 3 }
        return MeshFile(p, m.normals)
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
        // The profile's own bed shape (`machine.bed`, its points), when the engine made one.
        (_state.value.profileValues["machine.bed"] as? kotlinx.serialization.json.JsonArray)?.let { pts ->
            val xy = pts.mapNotNull { p ->
                (p as? kotlinx.serialization.json.JsonArray)?.mapNotNull { (it as? kotlinx.serialization.json.JsonPrimitive)?.content?.toFloatOrNull() }
            }.filter { it.size >= 2 }
            val w = xy.maxOfOrNull { it[0] } ?: 0f; val d = xy.maxOfOrNull { it[1] } ?: 0f
            if (w > 0 && d > 0) return floatArrayOf(w, d)
        }
        val choice = _state.value.profile
        val p = _state.value.catalog?.let { c -> c.printer(choice?.printer) ?: c.printer(choice?.name) }
        val v = p?.buildVolume
        return if (v != null && v.size >= 2 && v[0] > 0 && v[1] > 0) floatArrayOf(v[0].toFloat(), v[1].toFloat()) else floatArrayOf(220f, 220f)
    }

    /** Reads the model for the 3D view; a model it cannot read is still sliced by the engine. */
    private fun loadMesh(f: File) = viewModelScope.launch {
        val bed = bedSize()
        val mesh = withContext(io) { runCatching { MeshFile.read(f).onBed(bed[0] / 2, bed[1] / 2) }.getOrNull() }
        _state.update { it.copy(mesh = mesh, bed = bed, case = it.case.copy(conditions = emptyList()), stressMap = null, showStress = false) }
    }

    /** Picks the printer, nozzle and filament (the printer card). */
    fun setProfile(choice: EngineRunner.ProfileChoice?) {
        saved?.profilePrinter = choice?.printer
        choice?.let { saved?.profileNozzle = it.nozzle }
        saved?.profileFilament = choice?.filament
        saved?.profilePack = choice?.pack == true
        saved?.profileName = choice?.name
        _state.update { it.copy(profile = choice, result = null) }
        model?.let { loadMesh(it) }
        loadProfileValues()
    }
    val state: StateFlow<SliceState> = _state.asStateFlow()
    private val cancel = AtomicBoolean(false)
    private var model: File? = null

    fun dismissNotice() = _state.update { it.copy(notice = null) }

    /** The preview opened the G-code that came in. */
    fun gcodeOpened() = _state.update { it.copy(gcodeToPreview = null) }

    private fun isGcode(f: File): Boolean {
        if (f.extension.lowercase() in setOf("gcode", "bgcode", "gco", "g")) return true
        val head = runCatching { f.inputStream().use { i -> ByteArray(4).also { b -> if (i.read(b) < 4) return false } } }.getOrNull()
        return head != null && head.contentEquals(GcodeLayers.BINARY_MAGIC)
    }

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
        // QA AND5: a G-code (text or binary) is not a model: it opens in the preview.
        if (withContext(io) { isGcode(copied) }) {
            _state.update { it.copy(gcodeToPreview = copied.path) }
            return@launch
        }
        model = copied
        loadMesh(copied)
        _state.update {
            it.copy(modelName = name, isProject = copied.extension.equals("3mf", true), result = null, error = null, progress = 0,
                case = it.case.copy(conditions = emptyList()), marking = null, analysis = null)
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
                    if (s.isProject) runner.sliceProject(m, onProgress, { cancel.get() }, s.settings.changes)
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

    /** The offline help from the installed pack's `knowledge` (mobile_app.md, rule 16), read once. */
    private var knowledgeCache: Knowledge? = null
    fun knowledge(): Knowledge? = knowledgeCache ?: pack?.let { Knowledge.read(java.io.File(it.dataDir, "pack/knowledge")) }
        ?.takeIf { !it.empty }?.also { knowledgeCache = it }

    /** Downloads the pack's index for the wizard (MA1): its printers, before any vendor. */
    fun loadPackIndex() {
        val p = pack ?: return
        if (_state.value.packBusy) return
        _state.update { it.copy(packBusy = true, packError = null, packIndex = it.packIndex ?: p.index()) }
        viewModelScope.launch {
            val r = withContext(io) { runCatching { p.fetchIndex(packUrl) } }
            r.onFailure { AppLog.line("pack: index ${it.message}") }
            _state.update { s ->
                s.copy(packBusy = false, packIndex = r.getOrNull() ?: s.packIndex,
                    packError = r.exceptionOrNull()?.let { it.message ?: "error" }?.takeIf { s.packIndex == null || r.isFailure })
            }
        }
    }

    /** The pack's filaments for the chosen pack printer (rule 12), read off the main thread into the state. */
    fun loadPackFilaments() {
        val p = pack ?: return
        val choice = _state.value.profile?.takeIf { it.pack } ?: return
        viewModelScope.launch {
            val list = withContext(io) { runCatching { p.filamentsFor(choice.printer) }.getOrDefault(emptyList()) }
            _state.update { it.copy(packFilaments = list) }
        }
    }

    /** Picks a printer of the pack: downloads its vendor, installs it, and puts it on the printer card. */
    fun choosePackPrinter(vendor: ProfilePack.Vendor, model: ProfilePack.Model, variant: ProfilePack.Variant, done: () -> Unit) {
        val p = pack ?: return
        _state.update { it.copy(packBusy = true, packError = null, packProgress = 0) }
        viewModelScope.launch {
            val r = withContext(io) {
                runCatching {
                    p.update(listOf(vendor.id), packUrl, { pr -> _state.update { it.copy(packProgress = pr) } }) { false }
                }
            }
            r.onFailure { AppLog.line("pack: install ${it.message}") }
            _state.update { it.copy(packBusy = false, packError = r.exceptionOrNull()?.let { e -> e.message ?: "error" }) }
            if (r.isSuccess) {
                setProfile(EngineRunner.ProfileChoice(model.file + "#" + variant.id, variant.nozzle, null, pack = true, name = model.name))
                done()
            }
        }
    }

    /** Taps mark fixed faces or loaded faces, or (null) only turn the view. */
    fun setMarking(kind: EngineeringCase.Kind?) = _state.update { it.copy(marking = kind) }

    /** A tap on the 3D view along the ray [o] + t·[d]: the flat face under it becomes a condition. */
    fun pick(o: FloatArray, d: FloatArray) {
        val kind = state.value.marking ?: return
        val mesh = state.value.mesh ?: return
        viewModelScope.launch {
            val c = withContext(Dispatchers.Default) {
                val t = Picking.hit(mesh, o, d) ?: return@withContext null
                val face = Picking.flatFace(mesh, t)
                val n = Picking.normal(mesh, face)
                EngineeringCase.Condition(kind, face, Picking.box(mesh, face, EngineeringCase.pad(state.value.case.quality)),
                    direction = floatArrayOf(-n[0], -n[1], -n[2]), face = EngineeringCase.endFace(mesh, face, n))
            } ?: return@launch
            // A face already marked is not marked twice: the same kind takes it off, the other replaces it (QA AND7).
            _state.update {
                val same = it.case.conditions.indexOfFirst { o -> o.triangles.contentEquals(c.triangles) }
                val list = when {
                    same < 0 -> it.case.conditions + c
                    it.case.conditions[same].kind == c.kind -> it.case.conditions.filterIndexed { i, _ -> i != same }
                    else -> it.case.conditions.mapIndexed { i, o -> if (i == same) c else o }
                }
                it.copy(case = it.case.copy(conditions = list), analysis = null, stressMap = null, showStress = false)
            }
        }
    }

    fun removeCondition(index: Int) = _state.update {
        it.copy(case = it.case.copy(conditions = it.case.conditions.filterIndexed { i, _ -> i != index }), analysis = null,
            stressMap = null, showStress = false)
    }

    fun setCase(case: EngineeringCase) = _state.update { it.copy(case = case, analysis = null, stressMap = null, showStress = false) }

    fun showStress(on: Boolean) = _state.update { it.copy(showStress = on && it.stressMap != null) }

    /** Analyzes the bare model with the marked faces (docs/ANALYSIS.md, «El caso»). */
    fun analyzeModel() {
        val m = model ?: return
        val s = state.value
        val mesh = s.mesh ?: return
        if (s.slicing || s.analyzing || s.isProject) return
        cancel.set(false)
        _state.update { it.copy(analyzing = true, progress = 0, analysis = null, error = null, lightest = null, marking = null,
            stressMap = null, showStress = false) }
        viewModelScope.launch {
            try {
                val r = withContext(io) {
                    runner.analyzeModel(m, s.settings, s.case.toJson(mesh.offset), s.profile,
                        { p -> _state.update { it.copy(progress = p) } }) { cancel.get() }
                }
                val min = (r.summary["min_safety"] as? kotlinx.serialization.json.JsonPrimitive)?.content?.toDoubleOrNull()
                val valid = r.summary["valid"]?.toString() == "true"
                val verdict = when {
                    !valid || min == null -> "no_result"
                    min >= s.case.requiredSafety -> "holds"
                    else -> "fails"
                }
                val lines = listOf("verdict" to verdict, "required_safety" to s.case.requiredSafety.toString()) +
                    r.summary.mapNotNull { (k, v) -> (v as? kotlinx.serialization.json.JsonPrimitive)?.let { k to it.content } }
                        .filter { (k, _) -> k in setOf("min_safety", "max_displacement_mm", "max_von_mises_mpa") } +
                    (r.summary["warnings"] as? kotlinx.serialization.json.JsonArray).orEmpty()
                        .mapNotNull { (it as? kotlinx.serialization.json.JsonPrimitive)?.content }.map { "warning" to it }
                AppLog.line("engine: model analysis $verdict")
                val map = withContext(io) {
                    runCatching { StressMap.read(File(r.result.parentFile, "voxels.ply"), mesh.offset) }
                        .onFailure { AppLog.line("engine: no safety map ${it.message}") }.getOrNull()
                }
                _state.update { it.copy(analysis = lines, stressMap = map, showStress = map != null) }
            } catch (e: EngineRunner.Cancelled) {
                _state.update { it.copy(notice = SliceNotice.Cancelled) }
            } catch (e: EngineRunner.EngineException) {
                AppLog.line("engine: model analysis failed (${e.exitCode}) ${e.error}")
                _state.update { it.copy(error = e) }
            } catch (e: java.io.IOException) {
                _state.update { it.copy(error = EngineRunner.EngineException("error", e.message ?: "", "", -1)) }
            } finally {
                _state.update { it.copy(analyzing = false) }
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
        _state.update { it.copy(analyzing = true, progress = 0, analysis = null, error = null, lightest = null) }
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
                val found = study == Study.LIGHTEST && r.summary["found"]?.toString() == "true" && r.summary["solid"]?.toString() != "true"
                val density = if (found) (r.summary["density"] as? kotlinx.serialization.json.JsonPrimitive)?.content?.toDoubleOrNull()?.toInt() else null
                _state.update { it.copy(analysis = lines, lightest = density) }
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
