package net.tisma.app.engine

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.doubleOrNull
import kotlinx.serialization.json.put
import kotlinx.serialization.json.putJsonObject
import java.io.File
import java.util.concurrent.TimeUnit

/**
 * The engine of the desktop on the phone: the same `tisma-engine` program,
 * built with the NDK and packed in the APK (app/build.gradle.kts), run as a
 * separate process with the command line the cloud nodes use (cloud.md,
 * rule 18). Its progress comes on the standard output as `progress <n>`
 * lines; cancelling ends the process; a crash of the engine never takes the
 * app down. Own design, until the engine's C API for apps (mobile_app.md,
 * rule 2) takes its place.
 */
class EngineRunner(
    private val executable: File,
    private val workDir: File,
    /** Where the profile pack lives (`<packDir>/pack`), for its printers' profiles. */
    private val packDir: File? = null,
) {
    /** The engine's program, for the profile pack's commands. */
    val program: File get() = executable


    val available: Boolean get() = executable.isFile && executable.canExecute()

    /** A finished slice: the G-code and the summary of rule 18. */
    data class Slice(
        val gcode: File,
        val printTime: String,
        val printTimeS: Double?,
        val filamentMm: Double?,
        val filamentG: Double?,
        val layers: Int?,
        /** False when the printer's profile could not be made and the engine's defaults were used. */
        val profileUsed: Boolean = true,
    )

    class EngineException(val code: String, val error: String, val errorEs: String, val exitCode: Int) :
        Exception(error)

    class Cancelled : Exception("Cancelled")

    /** The printer and filament from the catalog, by the names the engine finds them by. */
    data class ProfileChoice(
        val printer: String,
        val nozzle: Double,
        val filament: String?,
        /** A printer of the profile pack, named `<model file>#<variant>`; [name] is what the person reads. */
        val pack: Boolean = false,
        val name: String? = null,
    )

    /** The simple settings of MOBILE.md, step 5: a quality (layer height) and supports or not. */
    data class SimpleSettings(
        val layerHeight: Double = 0.20,
        val supports: Boolean = false,
        /** The rest of what the person changed in Print and Material, by `layers.height` keys (mobile_app.md, rule 7). */
        val changes: Map<String, kotlinx.serialization.json.JsonElement> = emptyMap(),
    ) {
        fun toJson(): JsonObject = merge(SettingsLayout.nest(changes), buildJsonObject {
            putJsonObject("layers") { put("height", layerHeight) }
            putJsonObject("support") { put("enabled", supports) }
        })
    }

    /** Slices a 3MF project as the cloud nodes do: its own settings, sandboxed, on the CPU. */
    fun sliceProject(project: File, onProgress: (Int) -> Unit, cancelled: () -> Boolean,
                     changes: Map<String, kotlinx.serialization.json.JsonElement> = emptyMap()): Slice {
        workDir.mkdirs()
        val out = File(workDir, project.nameWithoutExtension + ".gcode")
        val summary = File(workDir, "summary.json")
        val args = mutableListOf("slice", project.path, "-o", out.path, "--summary", summary.path, "--sandbox", "--no-gpu")
        // The panel's changes over the project's own settings (`--config-over`, merged as RFC 7386).
        if (changes.isNotEmpty()) {
            val over = File(workDir, "config_over.json").apply { writeText(SettingsLayout.nest(changes).toString()) }
            args += listOf("--config-over", over.path)
        }
        return run(args, out, summary, onProgress, cancelled)
    }

    /** Slices a model (STL, OBJ, 3MF mesh) with [settings] (docs/CONFIG.md). */
    fun sliceModel(model: File, settings: SimpleSettings, onProgress: (Int) -> Unit, cancelled: () -> Boolean,
                   profile: ProfileChoice? = null): Slice {
        workDir.mkdirs()
        val base = profile?.let { profile(it) }
        val merged = if (base != null) merge(base, settings.toJson()) else settings.toJson()
        val config = File(workDir, "settings.json").apply { writeText(merged.toString()) }
        val out = File(workDir, model.nameWithoutExtension + ".gcode")
        val summary = File(workDir, "summary.json")
        return run(listOf("slice", model.path, "--config", config.path, "-o", out.path, "--summary", summary.path),
            out, summary, onProgress, cancelled).copy(profileUsed = profile == null || base != null)
    }

    /**
     * The settings the engine makes from its catalog for a printer, nozzle and
     * filament (`tisma-engine profile`), or null when it cannot (an engine
     * without the command, or a name it does not know): the slice then uses
     * the engine's defaults and says so.
     */
    fun profile(choice: ProfileChoice): JsonObject? {
        val out = File(workDir, "profile.json").apply { delete() }
        val args = if (choice.pack && packDir != null) {
            // The pack's variant with its own default filament and print profile (docs/CATALOG.md).
            mutableListOf(executable.path, "profile", "--pack", packDir.path, "--printer", choice.printer, "-o", out.path)
        } else mutableListOf(executable.path, "profile", "--printer", choice.printer,
            "--nozzle", choice.nozzle.toString(), "-o", out.path).also { a -> choice.filament?.let { a += listOf("--filament", it) } }
        return runCatching {
            val p = ProcessBuilder(args).directory(workDir).redirectErrorStream(true).start()
            p.outputStream.close()
            p.inputStream.readBytes()
            if (!p.waitFor(30, TimeUnit.SECONDS)) { p.destroyForcibly(); return null }
            if (p.exitValue() != 0 || !out.isFile) return null
            Json.parseToJsonElement(out.readText()) as? JsonObject
        }.getOrNull()
    }

    private fun run(args: List<String>, out: File, summary: File?, onProgress: (Int) -> Unit, cancelled: () -> Boolean): Slice {
        val s = runTool(args, out, summary, onProgress, cancelled)
        fun str(k: String) = (s?.get(k) as? JsonPrimitive)?.contentOrNull.orEmpty()
        fun num(k: String) = (s?.get(k) as? JsonPrimitive)?.takeIf { !it.isString }?.doubleOrNull
        return Slice(out, str("print_time"), num("print_time_s"), num("filament_mm"), num("filament_g"), num("layers")?.toInt())
    }

    /** An analysis's result: its file and the summary of cloud.md rules 19 and 19b. */
    data class Analysis(val result: File, val summary: JsonObject)

    /**
     * Analyzes whether the part holds, with the supports, loads and material saved
     * in the project (cloud.md, rule 19; mobile_app.md, rule 13): `analyze`.
     */
    fun analyze(project: File, objectIndex: Int, quality: String?, onProgress: (Int) -> Unit, cancelled: () -> Boolean): Analysis {
        val out = File(workDir, "result.tea")
        val summary = File(workDir, "summary.json")
        val args = mutableListOf("analyze", project.path, "-o", out.path, "--summary", summary.path, "--sandbox",
            "--mode", "analyze", "--object", objectIndex.toString(), "--instance", "0", "--no-gpu")
        quality?.let { args += listOf("--quality", it) }
        return Analysis(out, runTool(args, out, summary, onProgress, cancelled) ?: JsonObject(emptyMap()))
    }

    /**
     * The lowest infill that still holds (mobile_app.md, rule 13, «Relleno
     * mínimo»; cloud.md, rule 6, `lightest-infill`), without zones: the phone
     * can apply one density, not the zones' modifiers.
     */
    fun lightestInfill(project: File, objectIndex: Int, quality: String?, onProgress: (Int) -> Unit, cancelled: () -> Boolean): Analysis {
        val out = File(workDir, "result.tea")
        val summary = File(workDir, "summary.json")
        val args = mutableListOf("analyze", project.path, "-o", out.path, "--summary", summary.path, "--sandbox",
            "--mode", "lightest-infill", "--no-zones", "--object", objectIndex.toString(), "--instance", "0", "--no-gpu")
        quality?.let { args += listOf("--quality", it) }
        return Analysis(out, runTool(args, out, summary, onProgress, cancelled) ?: JsonObject(emptyMap()))
    }

    /**
     * Engineering on a bare model (docs/ANALYSIS.md): `analyze <model>
     * --config --case --progress`. The summary is the report's `result`, with
     * `valid`.
     */
    fun analyzeModel(model: File, settings: SimpleSettings, case: JsonObject, profile: ProfileChoice?,
                     onProgress: (Int) -> Unit, cancelled: () -> Boolean): Analysis {
        workDir.mkdirs()
        val base = profile?.let { profile(it) }
        val merged = if (base != null) merge(base, settings.toJson()) else settings.toJson()
        val config = File(workDir, "settings.json").apply { writeText(merged.toString()) }
        val caseFile = File(workDir, "case.json").apply { writeText(case.toString()) }
        val out = File(workDir, "report.json")
        runTool(listOf("analyze", model.path, "--config", config.path, "--case", caseFile.path, "-o", out.path, "--progress"),
            out, null, onProgress, cancelled, okCodes = setOf(0, 5)) // 5: no valid result, the report says why
        val report = runCatching { Json.parseToJsonElement(out.readText()) as JsonObject }.getOrNull() ?: JsonObject(emptyMap())
        val result = (report["result"] as? JsonObject) ?: JsonObject(emptyMap())
        // QA AND7: the engine's warnings say why there is no result (a load on a fixed face, nothing held).
        val warnings = (report["warnings"] as? kotlinx.serialization.json.JsonArray).orEmpty()
            .mapNotNull { ((it as? JsonObject)?.get("message") as? JsonPrimitive)?.contentOrNull }
        return Analysis(out, JsonObject(result + mapOf("valid" to (report["valid"] ?: JsonPrimitive(false)),
            "warnings" to kotlinx.serialization.json.JsonArray(warnings.map { JsonPrimitive(it) }))))
    }

    /** The air's drag on the part (cloud.md, rule 19b; aerodynamics.md): `aero`. */
    fun aero(project: File, objectIndex: Int, quality: String, speed: Double, direction: String,
             onProgress: (Int) -> Unit, cancelled: () -> Boolean): Analysis {
        val out = File(workDir, "result.taero")
        val summary = File(workDir, "summary.json")
        val args = listOf("aero", project.path, "-o", out.path, "--summary", summary.path, "--sandbox",
            "--object", objectIndex.toString(), "--instance", "0", "--quality", quality, "--speed", speed.toString(),
            "--direction", direction, "--no-gpu")
        return Analysis(out, runTool(args, out, summary, onProgress, cancelled) ?: JsonObject(emptyMap()))
    }

    /** Runs the engine with [args]; the summary, or an [EngineException] with its reason. */
    private fun runTool(args: List<String>, out: File, summary: File?, onProgress: (Int) -> Unit, cancelled: () -> Boolean,
                        okCodes: Set<Int> = setOf(0)): JsonObject? {
        workDir.mkdirs()
        out.delete()
        summary?.delete()
        val stderr = File(workDir, "engine.err")
        val process = ProcessBuilder(listOf(executable.path) + args)
            .directory(workDir)
            .redirectError(stderr)
            .start()
        process.outputStream.close()
        val reader = Thread {
            process.inputStream.bufferedReader().forEachLine { line ->
                // `analyze <model> --progress` says «progress: <stage> <0 to 1>».
                if (line.startsWith("progress: ")) line.substringAfterLast(' ').toDoubleOrNull()?.let { onProgress((it * 100).toInt().coerceIn(0, 100)) }
                if (line.startsWith("progress ")) line.removePrefix("progress ").trim().toDoubleOrNull()?.let {
                    onProgress(it.toInt().coerceIn(0, 100))
                }
            }
        }.apply { isDaemon = true; start() }
        while (!process.waitFor(100, TimeUnit.MILLISECONDS)) {
            if (cancelled()) {
                process.destroy()
                if (!process.waitFor(2, TimeUnit.SECONDS)) process.destroyForcibly()
                reader.join(1000)
                out.delete()
                throw Cancelled()
            }
        }
        reader.join(2000)
        val code = process.exitValue()
        val s = summary?.takeIf { it.isFile }?.let { runCatching { Json.parseToJsonElement(it.readText()) as JsonObject }.getOrNull() }
        fun str(k: String) = (s?.get(k) as? JsonPrimitive)?.contentOrNull.orEmpty()
        if (code !in okCodes || !out.isFile) {
            val err = str("error").ifBlank { stderr.takeIf { it.isFile }?.readText()?.trim()?.lines()?.lastOrNull().orEmpty() }
            throw EngineException(str("code"), err.ifBlank { "The engine stopped (exit code $code)" }, str("error_es"), code)
        }
        return s
    }

    companion object {
        /** [over] on top of [base], object by object (the simple choices win over the profile). */
        fun merge(base: JsonObject, over: JsonObject): JsonObject = JsonObject(base.toMutableMap().also { m ->
            for ((k, v) in over) {
                val b = m[k]
                m[k] = if (b is JsonObject && v is JsonObject) merge(b, v) else v
            }
        })

        /** The engine's file inside the app: Android unpacks it with the native libraries. */
        const val FILE_NAME = "libtisma_engine.so"
    }
}
