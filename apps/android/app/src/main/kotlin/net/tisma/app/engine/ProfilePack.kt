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
 * The free profile pack on the phone (profile_pack.md, rules 1 to 8), agreed with
 * the desktop session so its checks live once, in the engine: `pack plan` says
 * what to download, the app downloads it with Android's HTTPS (the engine on the
 * phone has no TLS), and `pack install` checks size and SHA-256, unpacks safely
 * and switches at once. Two rounds: the index first, then the vendors' archives.
 */
class ProfilePack(
    private val engine: File,
    private val dataDir: File,
    private val http: Http = UrlConnectionHttp,
) {
    data class Item(val url: String, val path: String, val size: Long, val sha256: String)

    class PackException(message: String) : Exception(message)

    /** Updates the pack: [vendors] empty means only the index (first start, MA1). */
    fun update(vendors: List<String>, url: String?, onProgress: (Int) -> Unit, cancelled: () -> Boolean) {
        repeat(2) { round ->
            val plan = plan(vendors, url)
            if (plan.isEmpty()) return
            val staging = File(dataDir.parentFile, "pack-staging").apply { deleteRecursively(); mkdirs() }
            plan.forEachIndexed { i, item ->
                if (cancelled()) throw EngineRunner.Cancelled()
                download(item, File(staging, item.path))
                onProgress(((round * 50) + 40 * (i + 1) / plan.size).coerceAtMost(99))
            }
            run(listOf("pack", "install", "--data", dataDir.path, "--from", staging.path))
            staging.deleteRecursively()
        }
        onProgress(100)
    }

    fun plan(vendors: List<String>, url: String?): List<Item> {
        val args = mutableListOf("pack", "plan", "--data", dataDir.path)
        if (vendors.isNotEmpty()) args += listOf("--vendors", vendors.joinToString(","))
        url?.let { args += listOf("--url", it) }
        val out = run(args)
        val array = runCatching { Json.parseToJsonElement(out) as JsonArray }.getOrNull()
            ?: throw PackException("Unexpected plan from the engine")
        return array.filterIsInstance<JsonObject>().map { o ->
            fun s(k: String) = (o[k] as? JsonPrimitive)?.contentOrNull.orEmpty()
            Item(s("url"), s("path"), (o["size"] as? JsonPrimitive)?.longOrNull ?: -1, s("sha256"))
        }
    }

    /** Downloads one file; only https (or the test server's http on this machine), only inside the staging folder. */
    private fun download(item: Item, target: File) {
        val staging = target.parentFile!!
        require(!item.path.contains("..") && !item.path.startsWith("/")) { "Bad path in the plan" }
        if (!item.url.startsWith("https://") && !item.url.startsWith("http://127.0.0.1")) throw PackException("Not an https address")
        staging.mkdirs()
        val r = try {
            http.send(HttpRequest("GET", item.url, mapOf("Accept" to "*/*"), timeoutMs = 120_000))
        } catch (e: IOException) {
            throw PackException("Cannot download ${item.path}: ${e.message}")
        }
        r.use {
            if (!it.ok) throw PackException("Cannot download ${item.path}: HTTP ${it.status}")
            target.outputStream().use { out -> it.body.copyTo(out, 64 * 1024) }
        }
        // The engine checks size and SHA-256 again; this only stops a wrong download early.
        if (item.size >= 0 && target.length() != item.size) throw PackException("${item.path} arrived incomplete")
    }

    private fun run(args: List<String>): String {
        dataDir.mkdirs()
        val p = ProcessBuilder(listOf(engine.path) + args).redirectErrorStream(false).start()
        p.outputStream.close()
        val out = p.inputStream.bufferedReader().readText()
        val err = p.errorStream.bufferedReader().readText()
        if (!p.waitFor(10, TimeUnit.MINUTES)) { p.destroyForcibly(); throw PackException("The engine took too long") }
        if (p.exitValue() != 0) throw PackException(err.trim().lines().lastOrNull()?.ifBlank { null } ?: "The engine stopped (${p.exitValue()})")
        return out.lines().filter { !it.startsWith("progress ") }.joinToString("\n")
    }
}
