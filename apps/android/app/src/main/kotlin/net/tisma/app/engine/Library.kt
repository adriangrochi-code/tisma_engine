package net.tisma.app.engine

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.booleanOrNull
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.doubleOrNull
import kotlinx.serialization.json.intOrNull
import kotlinx.serialization.json.longOrNull
import kotlinx.serialization.json.put
import java.io.File
import java.util.concurrent.TimeUnit

/**
 * «Mi biblioteca» (local_library.md): parts kept with their recipes and their
 * analyses, never their G-code. The entries' format lives in the engine
 * (`tisma-engine library`), the same on the desktop and the phone; this only
 * runs its commands and reads their JSON. Own design over the engine's
 * commands.
 */
class Library(private val engine: File, val dir: File) {
    class LibraryException(message: String, val exitCode: Int = 1, val answer: JsonObject? = null) : Exception(message)

    /** A line of the list (rule 4). */
    data class Item(
        val id: String, val name: String, val tags: List<String>, val printers: List<String>, val materials: List<String>,
        val recipes: Int, val analyses: Int, val bytes: Long, val modified: String, val thumb: File?,
    )

    /** A recipe (rule 1): the printer choice it was sliced with, the engine's version and its last slice's summary. */
    data class Recipe(
        val id: String, val choice: EngineRunner.ProfileChoice?, val label: String,
        val changes: Map<String, JsonElement>, val engine: String, val modified: String,
        val timeS: Double?, val filamentM: Double?, val filamentG: Double?, val layers: Int?,
    ) {
        val settings: String get() = "recipes/$id/settings.json"
    }

    data class Analysis(val id: String, val kind: String, val file: String, val summary: JsonObject)

    /** Rule 1, «La imprimí»: when, an optional photo (in the entry's photos/) and a note. */
    data class Printed(val date: String, val photo: String, val note: String)

    data class Entry(val id: String, val name: String, val tags: List<String>, val notes: String,
                     val recipes: List<Recipe>, val analyses: List<Analysis>, val printed: List<Printed> = emptyList())

    /** Rule 7: whether the recipe still matches, and which analyses still hold. */
    data class Check(val sameSettings: Boolean, val olderEngine: Boolean, val missingKeys: List<String>,
                     val analyses: Map<String, List<String>>) {
        fun valid(analysis: String) = analyses[analysis]?.isEmpty() ?: true
    }

    /** The engine has the `library` commands (an older engine does not: the screen is not offered). */
    val available: Boolean by lazy {
        engine.canExecute() && runCatching { run(listOf("library", "list", "--dir", dir.path)); true }.getOrDefault(false)
    }

    fun list(): List<Item> {
        val o = json(run(listOf("library", "list", "--dir", dir.path)))
        return (o["entries"] as? JsonArray).orEmpty().filterIsInstance<JsonObject>().map { e ->
            val id = e.s("id")
            Item(id, e.s("name"), e.list("tags"), e.list("printers"), e.list("materials"),
                e.i("recipes") ?: 0, e.i("analyses") ?: 0, (e["bytes"] as? JsonPrimitive)?.longOrNull ?: 0, e.s("modified"),
                e.s("thumb").takeIf { it.isNotBlank() }?.let { File(entryDir(id), it) }?.takeIf { it.isFile })
        }
    }

    fun show(id: String): Entry = entry(json(run(listOf("library", "show", "--dir", dir.path, "--entry", id))))

    /**
     * Rule 3: keeps the model with the settings it was sliced or analysed
     * with. [entry] null makes a new entry; otherwise the recipe or analysis
     * joins it.
     */
    fun save(entry: String?, model: File, config: File, choice: EngineRunner.ProfileChoice?, changes: Map<String, JsonElement>,
             label: String, work: File, summary: File? = null, analysis: File? = null, analysisSummary: JsonObject? = null,
             name: String? = null, thumb: File? = null): Entry {
        val recipe = File(work, "recipe.json").apply { writeText(recipeJson(choice, changes, label).toString()) }
        val args = mutableListOf("library", "save", "--dir", dir.path)
        args += if (entry == null) listOf("--new") else listOf("--entry", entry)
        args += listOf("--model", model.path, "--config", config.path, "--recipe", recipe.path)
        summary?.takeIf { it.isFile }?.let { args += listOf("--summary", it.path) }
        if (analysis != null && analysis.isFile) {
            args += listOf("--analysis", analysis.path)
            analysisSummary?.let { s -> args += listOf("--analysis-summary", File(work, "analysis_summary.json").apply { writeText(s.toString()) }.path) }
        }
        name?.let { args += listOf("--name", it) }
        thumb?.takeIf { it.isFile }?.let { args += listOf("--thumb", it.path) }
        return entry(json(run(args)))
    }

    fun check(entry: String, recipe: String, config: File): Check {
        val o = json(run(listOf("library", "check", "--dir", dir.path, "--entry", entry, "--recipe", recipe, "--config", config.path)))
        return Check((o["same_settings"] as? JsonPrimitive)?.booleanOrNull ?: false,
            (o["older_engine"] as? JsonPrimitive)?.booleanOrNull ?: false, o.list("missing_keys"),
            (o["analyses"] as? JsonArray).orEmpty().filterIsInstance<JsonObject>().associate { a ->
                a.s("id") to (if ((a["valid"] as? JsonPrimitive)?.booleanOrNull == false) a.list("changed").ifEmpty { listOf("?") } else emptyList())
            })
    }

    /** Rule 1: «La imprimí», with the date, an optional photo and a note. */
    fun printed(entry: String, date: String, photo: File? = null, note: String = ""): Entry {
        val args = mutableListOf("library", "printed", "--dir", dir.path, "--entry", entry, "--date", date)
        photo?.takeIf { it.isFile }?.let { args += listOf("--photo", it.path) }
        if (note.isNotBlank()) args += listOf("--note", note)
        return entry(json(run(args)))
    }

    fun export(entry: String, out: File): File {
        run(listOf("library", "export", "--dir", dir.path, "--entry", entry, "-o", out.path))
        return out
    }

    /** What an import did: the entry, or that one with that id is already there (rule 10: replace or keep both). */
    sealed interface Imported {
        data class Done(val entry: Entry) : Imported
        data class Exists(val id: String) : Imported
    }

    /** Rule 10 and ML6: the engine checks the file (no odd paths, no programs) before writing anything. */
    fun import(file: File, mode: String? = null): Imported {
        val args = mutableListOf("library", "import", file.path, "--dir", dir.path)
        when (mode) { "replace" -> args += "--replace"; "new" -> args += "--new" }
        return try {
            Imported.Done(entry(json(run(args))))
        } catch (e: LibraryException) {
            val id = e.answer?.s("exists")
            if (e.exitCode == 3 && !id.isNullOrBlank()) Imported.Exists(id) else throw e
        }
    }

    fun delete(entry: String, recipe: String? = null, analysis: String? = null) {
        val args = mutableListOf("library", "delete", "--dir", dir.path, "--entry", entry)
        recipe?.let { args += listOf("--recipe", it) }
        analysis?.let { args += listOf("--analysis", it) }
        run(args)
    }

    /** An entry's folder (rule 9): its project, thumbnail and recipes are read from there. */
    fun entryDir(id: String) = File(dir, id)

    private fun entry(o: JsonObject): Entry = Entry(
        o.s("id"), o.s("name"), o.list("tags"), o.s("notes"),
        (o["recipes"] as? JsonArray).orEmpty().filterIsInstance<JsonObject>().map { r ->
            val rec = r["recipe"] as? JsonObject ?: JsonObject(emptyMap())
            val sum = r["summary"] as? JsonObject
            Recipe(r.s("id"), choiceOf(rec), rec.s("label"), (rec["changes"] as? JsonObject).orEmpty(), r.s("engine"), r.s("modified"),
                sum?.d("time_s"), sum?.d("filament_m"), sum?.d("filament_g"), sum?.i("layers"))
        },
        (o["analyses"] as? JsonArray).orEmpty().filterIsInstance<JsonObject>().map { a ->
            Analysis(a.s("id"), a.s("kind"), a.s("file"), (a["summary"] as? JsonObject) ?: JsonObject(emptyMap()))
        },
        (o["printed"] as? JsonArray).orEmpty().filterIsInstance<JsonObject>().map { p -> Printed(p.s("date"), p.s("photo"), p.s("note")) },
    )

    private fun run(args: List<String>): String {
        dir.mkdirs()
        val err = File(dir.parentFile ?: dir, "library.err")
        val p = ProcessBuilder(listOf(engine.path) + args).redirectError(err).start()
        p.outputStream.close()
        val out = p.inputStream.bufferedReader().readText()
        if (!p.waitFor(5, TimeUnit.MINUTES)) { p.destroyForcibly(); throw LibraryException("The engine took too long") }
        val e = err.takeIf { it.isFile }?.readText().orEmpty().lines().filter { it.isNotBlank() }.lastOrNull()?.removePrefix("error: ")
        err.delete()
        if (p.exitValue() != 0) {
            val answer = runCatching { Json.parseToJsonElement(out) as JsonObject }.getOrNull()
            val message = answer?.s("error_es")?.ifBlank { null } ?: answer?.s("error")?.ifBlank { null } ?: e
                ?: "The engine stopped (${p.exitValue()})"
            throw LibraryException(message, p.exitValue(), answer)
        }
        return out
    }

    private fun json(text: String): JsonObject =
        runCatching { Json.parseToJsonElement(text) as JsonObject }.getOrElse { throw LibraryException("Unexpected answer from the engine") }

    companion object {
        /** The printer choice as the recipe keeps it: opaque to the engine, read back by the app (and the desktop). */
        fun recipeJson(choice: EngineRunner.ProfileChoice?, changes: Map<String, JsonElement>, label: String): JsonObject = buildJsonObject {
            if (choice != null) {
                put("printer", choice.printer); put("nozzle", choice.nozzle); put("pack", choice.pack)
                choice.filament?.let { put("filament", it) }
                choice.print?.let { put("print", it) }
                choice.name?.let { put("name", it) }
            }
            put("label", label)
            put("changes", JsonObject(changes))
        }

        fun choiceOf(r: JsonObject): EngineRunner.ProfileChoice? {
            val printer = r.s("printer").ifBlank { return null }
            return EngineRunner.ProfileChoice(printer, r.d("nozzle") ?: 0.4, r.s("filament").ifBlank { null },
                (r["pack"] as? JsonPrimitive)?.booleanOrNull ?: false, r.s("name").ifBlank { null }, r.s("print").ifBlank { null })
        }

        private fun JsonObject.s(k: String) = (this[k] as? JsonPrimitive)?.contentOrNull.orEmpty()
        private fun JsonObject.d(k: String) = (this[k] as? JsonPrimitive)?.doubleOrNull
        private fun JsonObject.i(k: String) = (this[k] as? JsonPrimitive)?.intOrNull
        private fun JsonObject.list(k: String) = (this[k] as? JsonArray).orEmpty().mapNotNull { (it as? JsonPrimitive)?.contentOrNull }
    }
}
