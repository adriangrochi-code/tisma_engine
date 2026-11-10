package net.tisma.app.engine

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.longOrNull
import net.tisma.cloud.Http
import net.tisma.cloud.HttpRequest
import net.tisma.cloud.UrlConnectionHttp
import java.io.File
import java.io.IOException
import java.util.concurrent.TimeUnit

/**
 * The free profile pack on the phone (profile_pack.md, rules 1 to 8;
 * docs/CATALOG.md, «El paquete libre de perfiles»). Its checks live once, in
 * the engine: `pack plan` says what to download, the app downloads it with
 * Android's HTTPS (the engine on the phone has no TLS), and `pack install`
 * checks size and SHA-256, unpacks safely and switches at once. Two plans:
 * the index first, then, with the downloaded index, the rest.
 */
class ProfilePack(
    private val engine: File,
    /** The pack lives in `<dataDir>/pack`. */
    val dataDir: File,
    private val http: Http = UrlConnectionHttp,
) {
    data class Item(val url: String, val name: String, val size: Long, val sha256: String)

    data class Plan(val downloads: List<Item>, val installed: String?, val published: String?)

    /** A printer of the pack: its name, and the `<file>#<variant>` the engine makes its profile from. */
    data class Printer(val vendor: String, val name: String, val nozzle: Double, val id: String)

    class PackException(message: String) : Exception(message)

    /** The pack's printers as its index lists them, before any vendor is downloaded (profile_pack.md, rule 2). */
    data class Index(val version: String, val vendors: List<Vendor>,
                     /** Vendors with filaments and no printers (filament libraries), installed with every printer. */
                     val libraries: List<String> = emptyList())
    data class Vendor(val id: String, val name: String, val models: List<Model>)
    data class Model(val name: String, val file: String, val variants: List<Variant>, val defaultVariant: String,
                     /** The bed's 3D model (STL) and picture (SVG or PNG), paths in the pack (rule 14). */
                     val bedModel: String = "", val bedTexture: String = "")
    data class Variant(val id: String, val nozzle: Double)

    /**
     * Downloads only the index (the first plan), keeps it as `<dataDir>/index.json`
     * for the wizard, and reads it.
     */
    fun fetchIndex(url: String?): Index {
        val first = plan(null, emptyList(), url)
        val item = first.downloads.firstOrNull { it.name == "index.json" } ?: throw PackException("The plan has no index")
        val target = File(dataDir, "index.json.part")
        download(item, target)
        val index = File(dataDir, "index.json")
        if (!target.renameTo(index)) { target.copyTo(index, overwrite = true); target.delete() }
        return readIndex(index) ?: throw PackException("The index cannot be read")
    }

    /** The installed pack's version, or null with none. */
    fun installedVersion(): String? = readIndex(File(dataDir, "pack/index.json"))?.version?.ifBlank { null }

    /**
     * Rule 19: the published index's version when it differs from the
     * installed one, or null (nothing installed, or the same).
     */
    fun newerVersion(url: String?): String? {
        val installed = installedVersion() ?: return null
        val published = fetchIndex(url).version
        return published.takeIf { it.isNotBlank() && it != installed }
    }

    /** The index of the installed pack, or the one the wizard downloaded, or null. */
    fun index(): Index? = readIndex(File(dataDir, "pack/index.json")) ?: readIndex(File(dataDir, "index.json"))

    companion object {
        /** Where the pack's profiles come from (rule 15; src/catalog/pack.hpp, kPackRepository). */
        const val REPOSITORY = "https://github.com/adriangrochi-code/Tisma-profiles-"
    }

    val installed: Boolean get() = File(dataDir, "pack/index.json").isFile

    /** A filament of the installed pack: the path the engine takes with `--filament`, and what the person reads. */
    data class Filament(val path: String, val name: String, val vendor: String, val generic: Boolean,
                        /** Rule 15: the entry's `license` and `origin`. */
                        val license: String = "", val origin: String = "",
                        /** profile_pack_format.md, filaments: the print profiles it works with; empty, any. */
                        val compatiblePrints: List<String> = emptyList(),
                        /** The material's type (PLA, PETG…), for a replacement of the same material. */
                        val material: String = "") {
        /** Whether it may be offered with the print profile [print] (its path; null: none known, so any). */
        fun worksWith(print: String?) = compatiblePrints.isEmpty() || print == null || print in compatiblePrints
    }

    /** Rule 15: `license · origin` of a pack file (a model, filament or print), or empty. */
    fun licenseLine(path: String): String {
        val o = runCatching { Json.parseToJsonElement(File(File(dataDir, "pack"), path.substringBefore('#')).readText()) as JsonObject }
            .getOrNull() ?: return ""
        return line(o)
    }

    private fun line(o: JsonObject): String = listOf("license", "origin")
        .mapNotNull { (o[it] as? JsonPrimitive)?.contentOrNull?.takeIf { v -> v.isNotBlank() } }.joinToString(" · ")

    /**
     * The installed pack's filaments for [printer] (`<file>#<variant>`), as
     * profile_pack.md, rule 12, says: only those whose `compatible.json` set
     * holds the printer (none: every printer), the printer's vendor's first,
     * then the generic ones, then the rest.
     */
    fun filamentsFor(printer: String): List<Filament> = entriesFor(printer, "filaments")

    /** The installed pack's print profiles for [printer], by the same rule 12 as the filaments. */
    fun printsFor(printer: String): List<Filament> = entriesFor(printer, "prints")

    private fun entriesFor(printer: String, folder: String): List<Filament> {
        val pack = File(dataDir, "pack")
        val index = runCatching { Json.parseToJsonElement(File(pack, "index.json").readText()) as JsonObject }.getOrNull() ?: return emptyList()
        val number = (index["printers"] as? JsonArray)?.indexOfFirst { (it as? JsonPrimitive)?.contentOrNull == printer } ?: -1
        val sets = runCatching {
            ((Json.parseToJsonElement(File(pack, "compatible.json").readText()) as JsonObject)["sets"] as JsonArray).map { set ->
                (set as JsonArray).mapNotNull { (it as? JsonPrimitive)?.contentOrNull?.toIntOrNull() }.toSet()
            }
        }.getOrDefault(emptyList())
        val printerVendor = printer.removePrefix("vendors/").substringBefore('/')
        val out = ArrayList<Filament>()
        File(pack, "vendors").listFiles()?.sortedBy { it.name }?.forEach { v ->
            File(v, folder).listFiles()?.filter { it.extension == "json" }?.sortedBy { it.name }?.forEach { f ->
                val o = runCatching { Json.parseToJsonElement(f.readText()) as JsonObject }.getOrNull() ?: return@forEach
                if ((o["technology"] as? JsonPrimitive)?.contentOrNull.let { it != null && it != "fff" }) return@forEach
                val set = (o["compatible"] as? JsonPrimitive)?.contentOrNull?.toIntOrNull()
                if (set != null && (number < 0 || sets.getOrNull(set)?.contains(number) != true)) return@forEach
                val generic = (o["generic"] as? JsonPrimitive)?.contentOrNull == "true"
                out += Filament("vendors/${v.name}/$folder/${f.name}", f.nameWithoutExtension.replace('_', ' '), v.name, generic,
                    (o["license"] as? JsonPrimitive)?.contentOrNull.orEmpty(), (o["origin"] as? JsonPrimitive)?.contentOrNull.orEmpty(),
                    (o["compatible_prints"] as? JsonArray).orEmpty().mapNotNull { (it as? JsonPrimitive)?.contentOrNull },
                    ((o["material"] as? JsonObject)?.get("type") as? JsonPrimitive)?.contentOrNull.orEmpty())
            }
        }
        return out.sortedBy { if (it.vendor == printerVendor && !it.generic) 0 else if (it.generic) 1 else 2 }
    }

    /**
     * Rule 13: the first filament of the variant's `defaults.filament` in the
     * installed model file, when it is installed, or null.
     */
    fun defaultFilament(printer: String): String? = defaultEntry(printer, "filament")

    /** Rule 13: the first print profile of the variant's `defaults.print`, or null. */
    fun defaultPrint(printer: String): String? = defaultEntry(printer, "print")

    private fun defaultEntry(printer: String, kind: String): String? {
        val file = printer.substringBefore('#')
        val variant = printer.substringAfter('#', "")
        val o = runCatching { Json.parseToJsonElement(File(File(dataDir, "pack"), file).readText()) as JsonObject }.getOrNull() ?: return null
        val v = (o["variants"] as? JsonArray).orEmpty().filterIsInstance<JsonObject>()
            .firstOrNull { (it["id"] as? JsonPrimitive)?.contentOrNull == variant } ?: return null
        val path = ((v["defaults"] as? JsonObject)?.get(kind) as? JsonArray)?.firstOrNull()
            ?.let { (it as? JsonPrimitive)?.contentOrNull } ?: return null
        return path.takeIf { File(File(dataDir, "pack"), it).isFile }
    }

    private fun readIndex(f: File): Index? {
        if (!f.isFile) return null
        val o = runCatching { Json.parseToJsonElement(f.readText()) as JsonObject }.getOrNull() ?: return null
        fun JsonObject.s(k: String) = (this[k] as? JsonPrimitive)?.contentOrNull.orEmpty()
        fun JsonObject.a(k: String) = (this[k] as? JsonArray).orEmpty().filterIsInstance<JsonObject>()
        val vendors = o.a("vendors").map { v ->
            Vendor(v.s("id"), v.s("name").ifBlank { v.s("id") }, v.a("models").filter { m ->
                m.s("technology").let { it.isBlank() || it == "fff" } // resin printers are not sliced on the phone
            }.map { m ->
                Model(m.s("name"), m.s("file"), m.a("variants").map { va ->
                    Variant(va.s("id"), va.s("nozzle").toDoubleOrNull() ?: 0.0)
                }, m.s("default_variant"), (m["assets"] as? JsonObject)?.s("bed_model").orEmpty(),
                    (m["assets"] as? JsonObject)?.s("bed_texture").orEmpty())
            })
        }.filter { it.models.isNotEmpty() }
        val libraries = o.a("vendors").filter { v ->
            v.a("models").isEmpty() && ((v["filaments"] as? JsonPrimitive)?.contentOrNull?.toIntOrNull() ?: 0) > 0
        }.map { it.s("id") }
        return Index(o.s("pack_version"), vendors, libraries)
    }

    /** Updates the pack: [vendors] empty means the installed ones, or all on the first start (MA1). Returns its version. */
    fun update(vendors: List<String>, url: String?, onProgress: (Int) -> Unit, cancelled: () -> Boolean): String? {
        val staging = File(dataDir, "pack-staging").apply { deleteRecursively(); mkdirs() }
        try {
            val first = plan(null, vendors, url)
            first.downloads.forEach { download(it, File(staging, it.name)) }
            onProgress(10)
            val index = File(staging, "index.json").takeIf { it.isFile } ?: throw PackException("The index did not arrive")
            val rest = plan(index, vendors, url)
            rest.downloads.forEachIndexed { i, item ->
                if (cancelled()) throw EngineRunner.Cancelled()
                download(item, File(staging, item.name))
                onProgress(10 + 80 * (i + 1) / rest.downloads.size.coerceAtLeast(1))
            }
            val report = run(listOf("pack", "install", "--data", dataDir.path, "--from", staging.path))
            onProgress(100)
            val o = runCatching { Json.parseToJsonElement(report) as JsonObject }.getOrNull()
            return (o?.get("pack_version") as? JsonPrimitive)?.contentOrNull ?: rest.published
        } finally {
            staging.deleteRecursively()
        }
    }

    fun plan(index: File?, vendors: List<String>, url: String?): Plan {
        val args = mutableListOf("pack", "plan", "--data", dataDir.path)
        index?.let { args += listOf("--index", it.path) }
        if (vendors.isNotEmpty()) args += listOf("--vendors", vendors.joinToString(","))
        url?.let { args += listOf("--url", it) }
        val o = runCatching { Json.parseToJsonElement(run(args)) as JsonObject }.getOrNull()
            ?: throw PackException("Unexpected plan from the engine")
        val list = (o["downloads"] as? JsonArray)?.filterIsInstance<JsonObject>().orEmpty().map { d ->
            fun s(k: String) = (d[k] as? JsonPrimitive)?.contentOrNull.orEmpty()
            Item(s("url"), s("name"), (d["size"] as? JsonPrimitive)?.longOrNull ?: 0, s("sha256"))
        }
        fun v(k: String) = (o[k] as? JsonPrimitive)?.contentOrNull
        return Plan(list, v("installed"), v("published"))
    }

    /** The printers of the installed pack (`pack list`), or empty when there is none. */
    fun printers(): List<Printer> {
        val text = runCatching { run(listOf("pack", "list", "--data", dataDir.path)) }.getOrNull() ?: return emptyList()
        val o = runCatching { Json.parseToJsonElement(text) as JsonObject }.getOrNull() ?: return emptyList()
        fun JsonObject.s(k: String) = (this[k] as? JsonPrimitive)?.contentOrNull.orEmpty()
        return (o["vendors"] as? JsonArray).orEmpty().filterIsInstance<JsonObject>().flatMap { v ->
            (v["models"] as? JsonArray).orEmpty().filterIsInstance<JsonObject>().flatMap { m ->
                (m["variants"] as? JsonArray).orEmpty().filterIsInstance<JsonObject>().map { va ->
                    Printer(v.s("id"), m.s("name"), (va["nozzle"] as? JsonPrimitive)?.contentOrNull?.toDoubleOrNull() ?: 0.0, va.s("printer"))
                }
            }
        }
    }

    /** Downloads one file; only https (or the test server's http on this machine), only inside the staging folder. */
    private fun download(item: Item, target: File) {
        require(item.name.isNotBlank() && !item.name.contains("..") && !item.name.contains('/') && !item.name.startsWith("\\")) {
            "Bad name in the plan"
        }
        if (!item.url.startsWith("https://") && !item.url.startsWith("http://127.0.0.1")) throw PackException("Not an https address")
        target.parentFile!!.mkdirs()
        val r = try {
            http.send(HttpRequest("GET", item.url, mapOf("Accept" to "*/*"), timeoutMs = 120_000))
        } catch (e: IOException) {
            throw PackException("Cannot download ${item.name}: ${e.message}")
        }
        r.use {
            if (!it.ok) throw PackException("Cannot download ${item.name}: HTTP ${it.status}")
            target.outputStream().use { out -> it.body.copyTo(out, 64 * 1024) }
        }
        // The engine checks size and SHA-256 again; this only stops a wrong download early (0: the plan does not know).
        if (item.size > 0 && target.length() != item.size) throw PackException("${item.name} arrived incomplete")
    }

    private fun run(args: List<String>): String {
        val c = callEngine(listOf(engine.path) + args, dataDir, dataDir, 600) ?: throw PackException("The engine took too long")
        if (c.exit != 0) throw PackException(c.lastError ?: "The engine stopped (${c.exit})")
        return c.out
    }
}
