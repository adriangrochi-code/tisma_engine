package net.tisma.app.engine

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.contentOrNull

/**
 * The offline help (mobile_app.md, rule 16; assistant.md): problems,
 * procedures, glossary, materials and calibration guides, from the profile
 * pack's `knowledge` folder. Read leniently: a missing file or key is left
 * out, so a newer pack does not break the app. Each part keeps its sources,
 * which the help shows.
 */
class Knowledge(
    val problems: List<Problem>,
    val procedures: List<Procedure>,
    val glossary: List<Term>,
    val materials: List<Material>,
    val calibrations: List<Calibration>,
    val sources: Map<String, List<String>>,
) {
    data class Action(val type: String, val id: String, val key: String, val value: JsonElement?, val why: String)
    data class Cause(val name: String, val explain: String, val check: String, val actions: List<Action>)
    data class Problem(val id: String, val name: String, val symptoms: String, val causes: List<Cause>)
    data class Step(val title: String, val text: String)
    data class Procedure(val id: String, val name: String, val forWhat: String, val steps: List<Step>)
    data class Term(val term: String, val definition: String)
    data class Material(val name: String, val lines: List<Pair<String, String>>)
    data class Calibration(val id: String, val name: String, val why: String, val steps: List<String>, val result: String)

    val empty get() = problems.isEmpty() && procedures.isEmpty() && glossary.isEmpty() && materials.isEmpty() && calibrations.isEmpty()

    companion object {
        /**
         * Reads the pack's `knowledge` folder through [file] (a name such as `problems.json` to its text, or null when
         * it is not there); an empty help when nothing is.
         */
        fun parse(file: (String) -> String?): Knowledge {
            fun load(name: String): JsonObject? = file("$name.json")?.let {
                runCatching { Json.parseToJsonElement(it) as JsonObject }.getOrNull()
            }
            fun JsonObject.s(k: String) = when (val v = this[k]) {
                is JsonPrimitive -> v.contentOrNull.orEmpty()
                is JsonArray -> v.joinToString(" – ") { (it as? JsonPrimitive)?.contentOrNull.orEmpty() }
                else -> ""
            }
            fun JsonObject.a(k: String) = (this[k] as? JsonArray).orEmpty().filterIsInstance<JsonObject>()
            fun JsonObject.strings(k: String) = (this[k] as? JsonArray).orEmpty().map {
                when (it) {
                    is JsonPrimitive -> it.contentOrNull.orEmpty()
                    is JsonObject -> listOf(it.s("title"), it.s("text")).filter(String::isNotBlank).joinToString(": ")
                    else -> ""
                }
            }.filter { it.isNotBlank() }
            val sources = HashMap<String, List<String>>()
            fun sourcesOf(name: String, o: JsonObject?) {
                o ?: return
                sources[name] = o.a("sources").map { listOf(it.s("title"), it.s("author")).filter(String::isNotBlank).joinToString(", ") }
            }
            val pr = load("problems").also { sourcesOf("problems", it) }
            val pc = load("procedures").also { sourcesOf("procedures", it) }
            val gl = load("glossary").also { sourcesOf("glossary", it) }
            val ma = load("materials").also { sourcesOf("materials", it) }
            val ca = load("calibrations").also { sourcesOf("calibrations", it) }
            return Knowledge(
                problems = pr?.a("problems").orEmpty().map { p ->
                    Problem(p.s("id"), p.s("name"), p.s("symptoms"), p.a("causes").map { c ->
                        Cause(c.s("name"), c.s("explain"), c.s("check"), c.a("actions").map { x ->
                            Action(x.s("type"), x.s("id"), x.s("key"), x["value"], x.s("why"))
                        })
                    })
                },
                procedures = pc?.a("procedures").orEmpty().map { p ->
                    Procedure(p.s("id"), p.s("name"), p.s("for"), p.a("steps").map { Step(it.s("title"), it.s("text")) })
                },
                glossary = gl?.a("terms").orEmpty().map { Term(it.s("term"), it.s("definition")) },
                materials = ma?.a("materials").orEmpty().map { m ->
                    Material(m.s("name"), listOf("nozzle_temp", "bed_temp", "fan", "bed_required", "enclosure", "nozzle_material", "surfaces")
                        .map { it to m.s(it) }.filter { it.second.isNotBlank() })
                },
                calibrations = ca?.a("calibrations").orEmpty().map { c ->
                    Calibration(c.s("id"), c.s("name"), c.s("why"), c.strings("steps"), c.s("result"))
                },
                sources = sources,
            )
        }
    }
}
