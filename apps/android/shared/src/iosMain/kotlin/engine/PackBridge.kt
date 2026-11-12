package net.tisma.app.engine

import net.tisma.cloud.HttpRequest
import net.tisma.cloud.PlatformHttp
import net.tisma.io.FileInput
import net.tisma.io.FileOutput
import net.tisma.io.IOException
import net.tisma.io.copyAll

/** A file the plan names, for Swift. */
data class PackFile(val url: String, val name: String, val size: Long)

/** A printer of the installed pack as the lists show it. */
data class PackPrinter(val vendor: String, val name: String, val nozzle: Double, val id: String)

/** One entry of the offline help, for the SwiftUI lists: a title, its text, and the lines under it. */
data class HelpEntry(val title: String, val text: String, val lines: List<String>)

/** One section of the offline help (problems, procedures, glossary, materials, calibration guides) and its sources. */
data class HelpSection(val id: String, val entries: List<HelpEntry>, val sources: List<String>)

/**
 * The free profile pack and its offline help on the iPhone (profile_pack.md; assistant.md; mobile_app.md, rules 11 and
 * 16; parity P10, P21), with the same commands and readings as Android ([PackData], [Knowledge]): the engine plans and
 * installs, this downloads what the plan names over HTTPS. Calls block: Swift calls them off the main thread.
 */
object PackBridge {
    fun planArgs(dataDir: String, index: String, vendors: String, url: String): List<String> =
        PackData.planArgs(dataDir, index.ifEmpty { null }, vendors.split(',').map { it.trim() }.filter { it.isNotEmpty() }, url.ifEmpty { null })

    fun installArgs(dataDir: String, staging: String): List<String> = PackData.installArgs(dataDir, staging)

    fun listArgs(dataDir: String): List<String> = PackData.listArgs(dataDir)

    fun profileArgs(packDir: String, printer: String, out: String): List<String> = PackData.profileArgs(packDir, printer, out, null, null)

    /** The files `pack plan` names; null when its answer is not a plan. */
    fun plan(json: String): List<PackFile>? = PackData.parsePlan(json)?.downloads?.map { PackFile(it.url, it.name, it.size) }

    fun printers(json: String): List<PackPrinter> = PackData.parseList(json).map { PackPrinter(it.vendor, it.name, it.nozzle, it.id) }

    /** Downloads one file of the plan into [dir] (only a plain name, only https); the engine checks its SHA-256. */
    @Throws(IOException::class)
    fun download(file: PackFile, dir: String) {
        if (!PackData.safe(PackData.Item(file.url, file.name, file.size, ""))) throw IOException("Not a file the plan may name: ${file.name}")
        val target = "$dir/${file.name}"
        val r = PlatformHttp.send(HttpRequest("GET", file.url, mapOf("Accept" to "*/*"), timeoutMs = 120_000, large = true))
        r.use {
            if (!it.ok) throw IOException("Cannot download ${file.name}: HTTP ${it.status}")
            FileOutput(target).use { out -> it.body.copyAll(out) }
        }
    }

    /** The help in the installed pack's `knowledge` folder at [dir]; sections without entries left out. */
    fun help(dir: String): List<HelpSection> {
        val k = Knowledge.parse { name -> runCatching { FileInput("$dir/$name").use { it.readText() } }.getOrNull() }
        fun src(id: String) = k.sources[id].orEmpty()
        return listOf(
            HelpSection("problems", k.problems.map { p ->
                HelpEntry(p.name, p.symptoms, p.causes.map { c -> listOf(c.name, c.explain, c.check).filter { it.isNotBlank() }.joinToString(": ") })
            }, src("problems")),
            HelpSection("procedures", k.procedures.map { p -> HelpEntry(p.name, p.forWhat, p.steps.map { s -> listOf(s.title, s.text).filter { it.isNotBlank() }.joinToString(": ") }) }, src("procedures")),
            HelpSection("glossary", k.glossary.map { HelpEntry(it.term, it.definition, emptyList()) }, src("glossary")),
            HelpSection("materials", k.materials.map { m -> HelpEntry(m.name, "", m.lines.map { "${it.first}: ${it.second}" }) }, src("materials")),
            HelpSection("calibrations", k.calibrations.map { c -> HelpEntry(c.name, c.why, c.steps + listOfNotNull(c.result.ifBlank { null })) }, src("calibrations")),
        ).filter { it.entries.isNotEmpty() }
    }

    private fun net.tisma.io.ByteInput.readText(): String {
        val out = net.tisma.io.BytesOutput()
        copyAll(out)
        return out.toByteArray().decodeToString()
    }
}
