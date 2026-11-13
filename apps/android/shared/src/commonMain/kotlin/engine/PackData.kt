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

    /** The installed pack's files, relative to its folder: Android reads them with java.io, the iPhone with posix. */
    interface Files {
        /** A file's text, or null when it is not there. */
        fun read(path: String): String?
        /** The names in a folder (files and folders), or empty. */
        fun list(dir: String): List<String>
    }

    /** A filament or print profile of the pack: the path the engine takes, and what the person reads. */
    data class Entry(val path: String, val name: String, val vendor: String, val generic: Boolean, val license: String,
                     val origin: String, val compatiblePrints: List<String>, val material: String)

    private fun obj(t: String?): JsonObject? = t?.let { runCatching { Json.parseToJsonElement(it) as? JsonObject }.getOrNull() }

    /**
     * The installed pack's filaments ([folder] `filaments`) or print profiles (`prints`) for [printer] (`<file>#<variant>`),
     * as profile_pack.md, rule 12, says: only those whose `compatible.json` set holds the printer (none: every printer),
     * the printer's vendor's first, then the generic ones, then the rest.
     */
    fun entriesFor(files: Files, printer: String, folder: String): List<Entry> {
        val index = obj(files.read("index.json")) ?: return emptyList()
        val number = (index["printers"] as? JsonArray)?.indexOfFirst { (it as? JsonPrimitive)?.contentOrNull == printer } ?: -1
        val sets = runCatching {
            ((obj(files.read("compatible.json"))!!["sets"]) as JsonArray).map { set ->
                (set as JsonArray).mapNotNull { (it as? JsonPrimitive)?.contentOrNull?.toIntOrNull() }.toSet()
            }
        }.getOrDefault(emptyList())
        val printerVendor = printer.removePrefix("vendors/").substringBefore('/')
        val out = ArrayList<Entry>()
        for (v in files.list("vendors").sorted()) {
            for (f in files.list("vendors/$v/$folder").filter { it.endsWith(".json") }.sorted()) {
                val o = obj(files.read("vendors/$v/$folder/$f")) ?: continue
                if ((o["technology"] as? JsonPrimitive)?.contentOrNull.let { it != null && it != "fff" }) continue
                val set = (o["compatible"] as? JsonPrimitive)?.contentOrNull?.toIntOrNull()
                if (set != null && (number < 0 || sets.getOrNull(set)?.contains(number) != true)) continue
                fun s(k: String) = (o[k] as? JsonPrimitive)?.contentOrNull.orEmpty()
                out += Entry("vendors/$v/$folder/$f", f.removeSuffix(".json").replace('_', ' '), v, s("generic") == "true",
                    s("license"), s("origin"), (o["compatible_prints"] as? JsonArray).orEmpty().mapNotNull { (it as? JsonPrimitive)?.contentOrNull },
                    ((o["material"] as? JsonObject)?.get("type") as? JsonPrimitive)?.contentOrNull.orEmpty())
            }
        }
        return out.sortedBy { if (it.vendor == printerVendor && !it.generic) 0 else if (it.generic) 1 else 2 }
    }

    /** Rule 13: the first entry of the variant's `defaults.<kind>` (filament or print) in its model file, when installed. */
    fun defaultEntry(files: Files, printer: String, kind: String): String? {
        val o = obj(files.read(printer.substringBefore('#'))) ?: return null
        val variant = printer.substringAfter('#', "")
        val v = (o["variants"] as? JsonArray).orEmpty().filterIsInstance<JsonObject>()
            .firstOrNull { (it["id"] as? JsonPrimitive)?.contentOrNull == variant } ?: return null
        val path = ((v["defaults"] as? JsonObject)?.get(kind) as? JsonArray)?.firstOrNull()
            ?.let { (it as? JsonPrimitive)?.contentOrNull } ?: return null
        return path.takeIf { files.read(it) != null }
    }
}
