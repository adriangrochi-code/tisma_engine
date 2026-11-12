package net.tisma.app.engine

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.longOrNull

/**
 * The free profile pack's commands and answers (profile_pack.md, rules 1 to 8; docs/CATALOG.md, «El paquete libre de
 * perfiles»), the same on Android and the iPhone: the engine plans and installs (`pack plan`, `pack install`, `pack
 * list`), the app only downloads what the plan names, over HTTPS. Own design.
 */
object PackData {
    data class Item(val url: String, val name: String, val size: Long, val sha256: String)

    data class Plan(val downloads: List<Item>, val installed: String?, val published: String?)

    /** A printer of the installed pack: its vendor, name, nozzle and the `<file>#<variant>` its profile comes from. */
    data class Printer(val vendor: String, val name: String, val nozzle: Double, val id: String)

    fun planArgs(dataDir: String, index: String?, vendors: List<String>, url: String?): List<String> =
        listOf("pack", "plan", "--data", dataDir) +
            (index?.let { listOf("--index", it) } ?: emptyList()) +
            (if (vendors.isNotEmpty()) listOf("--vendors", vendors.joinToString(",")) else emptyList()) +
            (url?.let { listOf("--url", it) } ?: emptyList())

    fun installArgs(dataDir: String, staging: String): List<String> = listOf("pack", "install", "--data", dataDir, "--from", staging)

    fun listArgs(dataDir: String): List<String> = listOf("pack", "list", "--data", dataDir)

    /** `profile --pack <pack> --printer <file#variant>`: the variant with its own default filament and print profile. */
    fun profileArgs(packDir: String, printer: String, out: String, filament: String?, print: String?): List<String> =
        listOf("profile", "--pack", packDir, "--printer", printer, "-o", out) +
            (filament?.let { listOf("--filament", it) } ?: emptyList()) + (print?.let { listOf("--print", it) } ?: emptyList())

    /** What `pack plan` prints; null when it is not a plan. */
    fun parsePlan(json: String): Plan? {
        val o = runCatching { Json.parseToJsonElement(json) as? JsonObject }.getOrNull() ?: return null
        val list = (o["downloads"] as? JsonArray)?.filterIsInstance<JsonObject>().orEmpty().map { d ->
            fun s(k: String) = (d[k] as? JsonPrimitive)?.contentOrNull.orEmpty()
            Item(s("url"), s("name"), (d["size"] as? JsonPrimitive)?.longOrNull ?: 0, s("sha256"))
        }
        fun v(k: String) = (o[k] as? JsonPrimitive)?.contentOrNull
        return Plan(list, v("installed"), v("published"))
    }

    /** What `pack list` prints: every variant of every model; empty when it is not a list. */
    fun parseList(json: String): List<Printer> {
        val o = runCatching { Json.parseToJsonElement(json) as? JsonObject }.getOrNull() ?: return emptyList()
        fun JsonObject.s(k: String) = (this[k] as? JsonPrimitive)?.contentOrNull.orEmpty()
        return (o["vendors"] as? JsonArray).orEmpty().filterIsInstance<JsonObject>().flatMap { v ->
            (v["models"] as? JsonArray).orEmpty().filterIsInstance<JsonObject>().flatMap { m ->
                (m["variants"] as? JsonArray).orEmpty().filterIsInstance<JsonObject>().map { va ->
                    Printer(v.s("id"), m.s("name"), (va["nozzle"] as? JsonPrimitive)?.contentOrNull?.toDoubleOrNull() ?: 0.0, va.s("printer"))
                }
            }
        }
    }

    /** A plan's file name is a plain name (no folders, no way up); its address is https, or the test server here. */
    fun safe(item: Item): Boolean =
        item.name.isNotBlank() && !item.name.contains("..") && !item.name.contains('/') && !item.name.startsWith("\\") &&
            (item.url.startsWith("https://") || item.url.startsWith("http://127.0.0.1"))
}
