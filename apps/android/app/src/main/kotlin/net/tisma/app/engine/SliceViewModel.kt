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
    /** The pack's print profiles for the chosen printer (rule 12). */
    val packPrints: List<ProfilePack.Filament> = emptyList(),
    /** Rule 19: a newer published pack, to offer its update. */
    val packNewer: String? = null,
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
    /** The plate's tools: the model's rotation (row-major 3×3, with mirrors) and its scale in %; a tap lays a face down. */
    val rotation: FloatArray = MeshFile.identity3(),
    val scalePercent: Float = 100f,
    val layFlat: Boolean = false,
    /** The analysis' stage now (estimate, slicing, voxels, solving, summary, resolution), to show with the bar. */
    val progressStage: String? = null,
    /** The estimate says the analysis would take this long on the phone (seconds): asked before running. */
    val slowAnalysis: Double? = null,
    /** What a tap on the 3D view marks now, or null when it only turns the view. */
    val marking: EngineeringCase.Kind? = null,
    /** The Design tab: the design, its last build (bodies, volume) and what the engine said. */
    val design: DesignDoc = DesignDoc(),
    val designMesh: MeshFile? = null,
    val designVolume: Double? = null,
    val designMessage: String? = null,
    val designBusy: Boolean = false,
    /** SolidCraft is in this build (Design needs it). */
    val solidcraft: Boolean = false,
    /** A calibration test on the plate: its kind, unit, segments (from, to, value) and its settings file. */
    val calibration: Calibrating? = null,
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

/** A calibration test on the plate (calibrations.md). */
data class Calibrating(val kind: String, val unit: String, val segments: List<Triple<Double, Double, Double>>,
                       val settings: File, val model: File)

sealed interface SliceNotice {
    data object Cancelled : SliceNotice
    data object CannotRead : SliceNotice
    data class Saved(val name: String) : SliceNotice
    /** Rule 20: the chosen pack printer is no longer in the pack; it stays chosen until the person changes it. */
    data object ProfileGone : SliceNotice
    data class PackUpdated(val version: String) : SliceNotice
    data class Calibrated(val kind: String, val value: Double) : SliceNotice
    /** Another profile replaced the printer settings, except these, changed by hand (BH27). */
    data class KeptChanges(val labels: List<String>) : SliceNotice
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
            EngineRunner.ProfileChoice(it, saved.profileNozzle, saved.profileFilament, saved.profilePack, saved.profileName,
                saved.profilePrint)
        },
        settings = savedSettings(),
        view = saved?.panelView?.let { v -> PanelVisibility.View.entries.firstOrNull { it.name.equals(v, true) } }
            ?: PanelVisibility.View.BASIC,
        custom = saved?.customVisible ?: emptySet(), packAvailable = pack != null, solidcraft = runner.solidcraft != null))

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
        runner.stageListener = { stage -> _state.update { it.copy(progressStage = stage) } }
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
    private fun loadProfileValues(announceKept: Boolean = false) {
        val choice = _state.value.profile
        if (choice == null || !runner.available) { _state.update { it.copy(profileValues = emptyMap()) }; return }
        viewModelScope.launch {
            val v = withContext(io) { runCatching { runner.profile(choice) }.getOrNull() }
            if (_state.value.profile == choice) {
                val values = v?.let { SettingsLayout.flatten(it) } ?: emptyMap()
                // BH27: the new profile's settings replace the old ones, except those changed by hand, which are said.
                val kept = if (announceKept) _state.value.settings.changes.keys.filter { it in values }.sorted() else emptyList()
                _state.update { it.copy(profileValues = values, notice = if (kept.isNotEmpty()) SliceNotice.KeptChanges(kept.map { k -> it.layout?.rows?.get(k)?.label ?: k }) else it.notice) }
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
        // Rule 14: a model much smaller than the bed is drawn anyway, and logged.
        if (b[3] - b[0] < bed[0] / 2 || b[4] - b[1] < bed[1] / 2) AppLog.line("bed: the bed model is much smaller than machine.bed")
        val dx = bed[0] / 2 - (b[0] + b[3]) / 2
        val dy = bed[1] / 2 - (b[1] + b[4]) / 2
        // A little under the grid, so the lines stay seen on its top.
        val dz = -0.05f
        val p = m.positions.copyOf()
        var i = 0
        while (i < p.size) { p[i] += dx; p[i + 1] += dy; p[i + 2] += dz; i += 3 }
        return MeshFile(p, m.normals)
    }

    /** The open model as read, in its file's coordinates, for the plate's tools. */
    private var original: MeshFile? = null
    private var originalFile: File? = null

    /** The plate's tools: rotation (and mirror) times the uniform scale. */
    private fun transform(): FloatArray = MeshFile.times(_state.value.rotation, MeshFile.scale(_state.value.scalePercent / 100f))

    private fun retransform(rotation: FloatArray = _state.value.rotation, scale: Float = _state.value.scalePercent) {
        _state.update { it.copy(rotation = rotation, scalePercent = scale, result = null) }
        model?.let { loadMesh(it) }
    }

    /** A quarter turn about X, Y or Z. */
    fun rotate(axis: Int) = retransform(MeshFile.times(MeshFile.quarterTurn(axis), _state.value.rotation))

    fun mirror(axis: Int) = retransform(MeshFile.times(MeshFile.mirror(axis), _state.value.rotation))

    fun setScale(percent: Float) { if (percent in 1f..10_000f) retransform(scale = percent) }

    fun resetTransform() = retransform(MeshFile.identity3(), 100f)

    /** The next tap on the part picks the face that goes down on the bed. */
    fun setLayFlat(on: Boolean) = _state.update { it.copy(layFlat = on, marking = if (on) null else it.marking) }

    private fun layFlatAt(o: FloatArray, d: FloatArray) {
        val mesh = _state.value.mesh ?: return
        viewModelScope.launch {
            val n = withContext(Dispatchers.Default) {
                Picking.hit(mesh, o, d)?.let { t -> Picking.normal(mesh, Picking.flatFace(mesh, t)) }
            } ?: return@launch
            _state.update { it.copy(layFlat = false) }
            retransform(MeshFile.times(MeshFile.layFlat(n), _state.value.rotation))
        }
    }

    /** What the engine reads: the model file, or the turned and scaled one written next to it. */
    private fun engineModel(m: File): File {
        val t = transform()
        if (t.contentEquals(MeshFile.identity3()) || _state.value.isProject) return m
        val o = original?.takeIf { originalFile == m } ?: MeshFile.read(m)
        return File(workDir, "plate_model.stl").also { o.transformed(t).writeStl(it) }
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
        val t = transform()
        val mesh = withContext(io) {
            runCatching {
                val o = original?.takeIf { originalFile == f } ?: MeshFile.read(f).also { original = it; originalFile = f }
                (if (t.contentEquals(MeshFile.identity3())) o else o.transformed(t)).onBed(bed[0] / 2, bed[1] / 2)
            }.getOrNull()
        }
        _state.update { it.copy(mesh = mesh, bed = bed, case = it.case.copy(conditions = emptyList()), stressMap = null, showStress = false) }
    }

    /** Picks the printer, nozzle and filament (the printer card). */
    fun setProfile(choice: EngineRunner.ProfileChoice?) {
        saved?.profilePrinter = choice?.printer
        choice?.let { saved?.profileNozzle = it.nozzle }
        saved?.profileFilament = choice?.filament
        saved?.profilePack = choice?.pack == true
        saved?.profileName = choice?.name
        saved?.profilePrint = choice?.print
        _state.update { it.copy(profile = choice, result = null) }
        model?.let { loadMesh(it) }
        loadProfileValues(announceKept = true)
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
        original = null; originalFile = null
        _state.update {
            it.copy(modelName = name, isProject = copied.extension.equals("3mf", true), result = null, error = null, progress = 0,
                case = it.case.copy(conditions = emptyList()), marking = null, analysis = null,
                rotation = MeshFile.identity3(), scalePercent = 100f, layFlat = false, calibration = null, stressMap = null, showStress = false)
        }
        loadMesh(copied)
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
                    else runner.sliceModel(engineModel(m), s.settings, onProgress, { cancel.get() }, s.profile,
                        s.calibration?.settings?.takeIf { s.calibration.model == m })
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

    fun setDesign(doc: DesignDoc) = _state.update { it.copy(design = doc, designMessage = null) }

    /**
     * Builds the design (cad_design.md): the engine checks it, then builds it
     * through SolidCraft as a timeline; the bodies are shown, or the first
     * feature that fails and why.
     */
    fun buildDesign() {
        val sc = runner.solidcraft ?: return
        val doc = state.value.design
        if (state.value.designBusy) return
        _state.update { it.copy(designBusy = true, designMessage = null) }
        viewModelScope.launch {
            val r = withContext(io) {
                runCatching {
                    val check = runner.designCheck(doc)
                    val problems = (check["problems"] as? kotlinx.serialization.json.JsonArray).orEmpty()
                        .mapNotNull { (it as? kotlinx.serialization.json.JsonPrimitive)?.content }
                    if (problems.isNotEmpty()) return@runCatching Triple<MeshFile?, Double?, String?>(null, null, problems.joinToString("\n"))
                    val (stl, o) = runner.designBuild(doc, sc)
                    fun s(k: String) = (o[k] as? kotlinx.serialization.json.JsonPrimitive)?.content
                    val mesh = stl?.let { MeshFile.read(it) }
                    val failure = s("failure")?.takeIf { it.isNotBlank() && it != "null" }
                        ?.let { f -> (s("failed_feature")?.toIntOrNull()?.let { doc.features.getOrNull(it)?.name + ": " } ?: "") + f }
                        ?: s("error")?.takeIf { s("ok") != "true" }
                    Triple(mesh, s("volume_mm3")?.toDoubleOrNull(), failure)
                }
            }
            r.onFailure { AppLog.line("design: ${it.message}") }
            _state.update {
                it.copy(designBusy = false, designMesh = r.getOrNull()?.first, designVolume = r.getOrNull()?.second,
                    designMessage = r.getOrNull()?.third ?: r.exceptionOrNull()?.let { e -> (e as? EngineRunner.EngineException)?.error ?: e.message })
            }
        }
    }

    /** The built bodies go to the plate as a model. */
    fun designToPlate() {
        val m = state.value.designMesh ?: return
        viewModelScope.launch {
            val f = withContext(io) { File(workDir, "design_plate.stl").also { m.writeStl(it) } }
            open(state.value.design.name.replace(Regex("[^A-Za-z0-9 ._-]"), "_") + ".stl") { f.inputStream() }
        }
    }

    /** A Design part's starting spec, as the engine gives it (`part … --defaults`). */
    fun partDefaults(kind: String, done: (kotlinx.serialization.json.JsonObject?) -> Unit) = viewModelScope.launch {
        done(withContext(io) { runCatching { runner.partDefaults(kind) }.getOrNull() })
    }

    /** Makes a gear or a fastener with the engine and opens it on the plate; a refused spec gives its reason. */
    fun makePart(kind: String, spec: kotlinx.serialization.json.JsonObject, done: (String?) -> Unit) = viewModelScope.launch {
        val r = withContext(io) { runCatching { runner.makePart(kind, spec, kind) } }
        val e = r.exceptionOrNull()
        if (e != null) { done((e as? EngineRunner.EngineException)?.error ?: e.message ?: "error"); return@launch }
        val stl = r.getOrThrow().first
        open("$kind.stl") { stl.inputStream() }
        done(null)
    }

    /** Makes a calibration test (calibrations.md) and opens it on the plate with its own settings. */
    fun calibrate(kind: String, range: DoubleArray?, done: (String?) -> Unit) = viewModelScope.launch {
        val s = state.value
        val r = withContext(io) { runCatching { runner.calibration(kind, s.settings, s.profile, range) } }
        val e = r.exceptionOrNull()
        if (e != null) { done((e as? EngineRunner.EngineException)?.error ?: e.message ?: "error"); return@launch }
        val c = r.getOrThrow()
        // The test's model is opened like any other, then its settings are tied to it.
        open("calibration_$kind.stl") { c.model.inputStream() }.join()
        _state.update { it.copy(calibration = Calibrating(kind, c.unit, c.segments, c.settings, model ?: c.model)) }
        done(null)
    }

    /** The value that printed best goes into the panel's changes (only what it changes), and the test is closed. */
    fun applyCalibration(value: Double) {
        val c = state.value.calibration ?: return
        val s = state.value
        viewModelScope.launch {
            val r = withContext(io) {
                runCatching {
                    val before = SettingsLayout.flatten(kotlinx.serialization.json.Json.parseToJsonElement(
                        runner.writeConfig(s.settings, s.profile).readText()) as kotlinx.serialization.json.JsonObject)
                    val after = SettingsLayout.flatten(runner.calibrationApply(c.kind, value, s.settings, s.profile))
                    after.filter { (k, v) -> v !is kotlinx.serialization.json.JsonObject && before[k] != v && k != "changes" }
                }
            }
            r.onFailure { AppLog.line("calibration: apply ${it.message}") }
            r.getOrNull()?.forEach { (k, v) -> setValue(k, v) }
            _state.update { it.copy(calibration = null, notice = if (r.isSuccess) SliceNotice.Calibrated(c.kind, value) else it.notice,
                error = r.exceptionOrNull()?.let { e -> (e as? EngineRunner.EngineException) }) }
        }
    }

    fun closeCalibration() = _state.update { it.copy(calibration = null) }

    /** Turns the bare model to the least-support orientation the engine finds (orientation.md). */
    fun orientAutomatically() {
        val m = model ?: return
        if (state.value.slicing || state.value.analyzing || state.value.isProject) return
        cancel.set(false)
        _state.update { it.copy(analyzing = true, progress = 0) }
        val s = state.value
        viewModelScope.launch {
            val r = withContext(io) {
                runCatching { runner.orientModel(engineModel(m), s.settings, s.profile, { p -> _state.update { it.copy(progress = p) } }) { cancel.get() } }
            }
            r.onFailure { AppLog.line("orient: ${it.message}") }
            _state.update { it.copy(analyzing = false, error = r.exceptionOrNull() as? EngineRunner.EngineException) }
            // The engine turned the already turned model: its rotation goes on top of the plate's.
            r.getOrNull()?.let { (rot, _) -> retransform(MeshFile.times(rot, _state.value.rotation)) }
        }
    }

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
            val prints = withContext(io) { runCatching { p.printsFor(choice.printer) }.getOrDefault(emptyList()) }
            _state.update { it.copy(packFilaments = list, packPrints = prints) }
        }
    }

    /** Rule 19: on opening, at most once a day, whether a newer pack is published. */
    fun checkPackOnOpen(now: Long = System.currentTimeMillis()) {
        val p = pack ?: return
        val s = saved ?: return
        if (now - s.packLastCheck < 24L * 3600 * 1000) return
        viewModelScope.launch {
            val r = withContext(io) { runCatching { p.newerVersion(packUrl) } }
            r.onFailure { AppLog.line("pack: check ${it.message}") }
            if (r.isSuccess) s.packLastCheck = now
            _state.update { it.copy(packNewer = r.getOrNull()) }
        }
    }

    /** Rule 20: the installed vendors whose archive changed; the chosen printer is checked afterwards. */
    fun updatePack() {
        val p = pack ?: return
        if (_state.value.packBusy) return
        _state.update { it.copy(packBusy = true, packProgress = 0, packError = null) }
        viewModelScope.launch {
            val r = withContext(io) {
                runCatching {
                    val v = p.update(emptyList(), packUrl, { pr -> _state.update { it.copy(packProgress = pr) } }) { false }
                    val choice = _state.value.profile?.takeIf { it.pack }
                    v to (choice != null && p.printers().none { it.id == choice.printer })
                }
            }
            r.onFailure { AppLog.line("pack: update ${it.message}") }
            knowledgeCache = null
            _state.update {
                it.copy(packBusy = false, packError = r.exceptionOrNull()?.message,
                    packNewer = if (r.isSuccess) null else it.packNewer,
                    notice = r.getOrNull()?.let { (v, gone) -> if (gone) SliceNotice.ProfileGone else SliceNotice.PackUpdated(v ?: "") })
            }
        }
    }

    /** Picks a printer of the pack: downloads its vendor, installs it, and puts it on the printer card. */
    fun choosePackPrinter(vendor: ProfilePack.Vendor, model: ProfilePack.Model, variant: ProfilePack.Variant, done: () -> Unit) {
        val p = pack ?: return
        _state.update { it.copy(packBusy = true, packError = null, packProgress = 0) }
        viewModelScope.launch {
            val r = withContext(io) {
                runCatching {
                    // The filament libraries come with every printer, so the generic filaments are there too.
                    val libraries = (_state.value.packIndex ?: p.index())?.libraries.orEmpty()
                    p.update(listOf(vendor.id) + libraries, packUrl, { pr -> _state.update { it.copy(packProgress = pr) } }) { false }
                }
            }
            r.onFailure { AppLog.line("pack: install ${it.message}") }
            _state.update { it.copy(packBusy = false, packError = r.exceptionOrNull()?.let { e -> e.message ?: "error" }) }
            if (r.isSuccess) {
                val printer = model.file + "#" + variant.id
                val filament = withContext(io) { runCatching { p.defaultFilament(printer) }.getOrNull() }
                val print = withContext(io) { runCatching { p.defaultPrint(printer) }.getOrNull() }
                setProfile(EngineRunner.ProfileChoice(printer, variant.nozzle, filament, pack = true, name = model.name, print = print))
                done()
            }
        }
    }

    /** Taps mark fixed faces or loaded faces, or (null) only turn the view. */
    fun setMarking(kind: EngineeringCase.Kind?) = _state.update { it.copy(marking = kind) }

    /** A tap on the 3D view along the ray [o] + t·[d]: the flat face under it becomes a condition. */
    fun pick(o: FloatArray, d: FloatArray) {
        if (_state.value.layFlat) { layFlatAt(o, d); return }
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

    /**
     * How much slower than the engine's 4-core reference machine a phone is
     * assumed to be, for the estimate (own guess until measured on phones,
     * mobile_app.md rule 18).
     */
    private val phoneFactor = 4.0

    /** Analyzes the bare model with the marked faces (docs/ANALYSIS.md, «El caso»); first estimates its time. */
    fun analyzeModel(force: Boolean = false) {
        val m = model ?: return
        val s = state.value
        val mesh = s.mesh ?: return
        if (s.slicing || s.analyzing || s.isProject) return
        if (!force) {
            _state.update { it.copy(analyzing = true, progress = 0, progressStage = "estimate", slowAnalysis = null) }
            viewModelScope.launch {
                val e = withContext(io) { runCatching { runner.estimateModel(engineModel(m), s.settings, s.case.toJson(mesh.offset), s.profile) } }
                e.onFailure { AppLog.line("engine: estimate ${it.message}") }
                val seconds = (e.getOrNull()?.first ?: 0.0) * phoneFactor
                _state.update { it.copy(analyzing = false, progressStage = null) }
                // More than a minute on the phone: say so first, with the quick option (QA, 04-11-2026).
                if (seconds > 60) _state.update { it.copy(slowAnalysis = seconds) } else analyzeModel(force = true)
            }
            return
        }
        _state.update { it.copy(slowAnalysis = null) }
        cancel.set(false)
        _state.update { it.copy(analyzing = true, progress = 0, analysis = null, error = null, lightest = null, marking = null,
            stressMap = null, showStress = false) }
        viewModelScope.launch {
            try {
                val r = withContext(io) {
                    runner.analyzeModel(engineModel(m), s.settings, s.case.toJson(mesh.offset), s.profile,
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
                _state.update { it.copy(analyzing = false, progressStage = null) }
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
        val s0 = state.value
        viewModelScope.launch {
            try {
                val r = withContext(io) {
                    val onProgress = { p: Int -> _state.update { it.copy(progress = p) } }
                    when (study) {
                        Study.STRUCTURE -> runner.analyze(m, objectIndex, quality, onProgress) { cancel.get() }
                        Study.LIGHTEST -> runner.lightestInfill(m, objectIndex, quality, onProgress) { cancel.get() }
                        Study.AERO -> if (s0.isProject) runner.aero(m, objectIndex, quality ?: "fast", speed, direction, onProgress) { cancel.get() }
                            // A bare model: `aero <model> --config` with the plate's settings.
                            else runner.aeroModel(engineModel(m), s0.settings, s0.profile, quality ?: "fast", speed, direction, onProgress) { cancel.get() }
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
                _state.update { it.copy(analyzing = false, progressStage = null) }
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
