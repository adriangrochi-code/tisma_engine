package net.tisma.app.data

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.contentOrNull

/**
 * «Credits and sources» (docs/spec/features/credits.md, rule 4): the functions
 * «based on» a source, from the engine's resources/credits.json (the desktop's
 * same data), and the libraries the APK carries with their notices: the
 * engine's ones it links and the app's own (credits_android.json).
 */
class Credits(val features: List<Feature>, val libraries: List<Library>) {
    data class Feature(val name: String, val source: String, val author: String, val license: String, val url: String)
    data class Library(val name: String, val version: String, val license: String, val url: String, val notices: List<String>,
                       val gradle: List<String> = emptyList())

    companion object {
        fun parse(engineJson: String, androidJson: String, spanish: Boolean): Credits {
            val engine = Json.parseToJsonElement(engineJson) as JsonObject
            val app = Json.parseToJsonElement(androidJson) as JsonObject
            fun JsonObject.s(k: String): String = when (val v = this[k]) {
                is JsonPrimitive -> v.contentOrNull.orEmpty()
                is JsonObject -> v.s(if (spanish) "es" else "en")
                else -> ""
            }
            fun JsonObject.list(k: String) = (this[k] as? JsonArray)?.mapNotNull { (it as? JsonPrimitive)?.contentOrNull } ?: emptyList()
            fun lib(o: JsonObject) = Library(o.s("name"), o.s("version"), o.s("license"), o.s("url"), o.list("notices"), o.list("gradle"))
            val features = (engine["features"] as? JsonArray)?.filterIsInstance<JsonObject>()?.map {
                Feature(it.s("feature"), it.s("source"), it.s("author"), it.s("license"), it.s("url"))
            } ?: emptyList()
            val inApk = app.list("engine_libraries").toSet()
            val engineLibs = (engine["libraries"] as? JsonArray)?.filterIsInstance<JsonObject>()?.map(::lib)
                ?.filter { it.name in inApk } ?: emptyList()
            val appLibs = (app["libraries"] as? JsonArray)?.filterIsInstance<JsonObject>()?.map(::lib) ?: emptyList()
            return Credits(features, engineLibs + appLibs)
        }
    }
}
