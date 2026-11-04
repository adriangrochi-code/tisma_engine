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
    data class Index(val version: String, val vendors: List<Vendor>)
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

    val installed: Boolean get() = File(dataDir, "pack/index.json").isFile

    /** A filament of the installed pack: the path the engine takes with `--filament`, and what the person reads. */
    data class Filament(val path: String, val name: String, val vendor: String, val generic: Boolean)

    /**
     * The installed pack's filaments for [printer] (`<file>#<variant>`), as
     * profile_pack.md, rule 12, says: only those whose `compatible.json` set
     * holds the printer (none: every printer), the printer's vendor's first,
     * then the generic ones, then the rest.
     */
    fun filamentsFor(printer: String): List<Filament> {
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
            File(v, "filaments").listFiles()?.filter { it.extension == "json" }?.sortedBy { it.name }?.forEach { f ->
                val o = runCatching { Json.parseToJsonElement(f.readText()) as JsonObject }.getOrNull() ?: return@forEach
                if ((o["technology"] as? JsonPrimitive)?.contentOrNull.let { it != null && it != "fff" }) return@forEach
                val set = (o["compatible"] as? JsonPrimitive)?.contentOrNull?.toIntOrNull()
                if (set != null && (number < 0 || sets.getOrNull(set)?.contains(number) != true)) return@forEach
                val generic = (o["generic"] as? JsonPrimitive)?.contentOrNull == "true"
                out += Filament("vendors/${v.name}/filaments/${f.name}", f.nameWithoutExtension.replace('_', ' '), v.name, generic)
            }
        }
        return out.sortedBy { if (it.vendor == printerVendor && !it.generic) 0 else if (it.generic) 1 else 2 }
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
        return Index(o.s("pack_version"), vendors)
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
        dataDir.mkdirs()
        val err = File(dataDir, "pack.err")
        val p = ProcessBuilder(listOf(engine.path) + args).redirectError(err).start()
        p.outputStream.close()
        val out = p.inputStream.bufferedReader().readText()
        if (!p.waitFor(10, TimeUnit.MINUTES)) { p.destroyForcibly(); throw PackException("The engine took too long") }
        val e = err.takeIf { it.isFile }?.readText().orEmpty().lines().filter { it.isNotBlank() && !it.startsWith("progress") }
        err.delete()
        if (p.exitValue() != 0) throw PackException(e.lastOrNull()?.removePrefix("error: ") ?: "The engine stopped (${p.exitValue()})")
        return out
    }
}
