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
class EngineRunner(private val executable: File, private val workDir: File) {

    val available: Boolean get() = executable.isFile && executable.canExecute()

    /** A finished slice: the G-code and the summary of rule 18. */
    data class Slice(
        val gcode: File,
        val printTime: String,
        val printTimeS: Double?,
        val filamentMm: Double?,
        val filamentG: Double?,
        val layers: Int?,
    )

    class EngineException(val code: String, val error: String, val errorEs: String, val exitCode: Int) :
        Exception(error)

    class Cancelled : Exception("Cancelled")

    /** The simple settings of MOBILE.md, step 5: a quality (layer height) and supports or not. */
    data class SimpleSettings(val layerHeight: Double = 0.20, val supports: Boolean = false) {
        fun toJson(): JsonObject = buildJsonObject {
            putJsonObject("layers") { put("height", layerHeight) }
            putJsonObject("support") { put("enabled", supports) }
        }
    }

    /** Slices a 3MF project as the cloud nodes do: its own settings, sandboxed, on the CPU. */
    fun sliceProject(project: File, onProgress: (Int) -> Unit, cancelled: () -> Boolean): Slice {
        val out = File(workDir, project.nameWithoutExtension + ".gcode")
        val summary = File(workDir, "summary.json")
        return run(listOf("slice", project.path, "-o", out.path, "--summary", summary.path, "--sandbox", "--no-gpu"),
            out, summary, onProgress, cancelled)
    }

    /** Slices a model (STL, OBJ, 3MF mesh) with [settings] (docs/CONFIG.md). */
    fun sliceModel(model: File, settings: SimpleSettings, onProgress: (Int) -> Unit, cancelled: () -> Boolean): Slice {
        val config = File(workDir, "settings.json").apply { writeText(settings.toJson().toString()) }
        val out = File(workDir, model.nameWithoutExtension + ".gcode")
        return run(listOf("slice", model.path, "--config", config.path, "-o", out.path), out, null, onProgress, cancelled)
    }

    private fun run(args: List<String>, out: File, summary: File?, onProgress: (Int) -> Unit, cancelled: () -> Boolean): Slice {
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
        fun num(k: String) = (s?.get(k) as? JsonPrimitive)?.takeIf { !it.isString }?.doubleOrNull
        if (code != 0 || !out.isFile) {
            val err = str("error").ifBlank { stderr.takeIf { it.isFile }?.readText()?.trim()?.lines()?.lastOrNull().orEmpty() }
            throw EngineException(str("code"), err.ifBlank { "The engine stopped (exit code $code)" }, str("error_es"), code)
        }
        return Slice(out, str("print_time"), num("print_time_s"), num("filament_mm"), num("filament_g"), num("layers")?.toInt())
    }

    companion object {
        /** The engine's file inside the app: Android unpacks it with the native libraries. */
        const val FILE_NAME = "libtisma_engine.so"
    }
}
