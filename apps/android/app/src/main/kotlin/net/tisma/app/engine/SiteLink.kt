package net.tisma.app.engine

import java.io.File
import java.io.IOException
import java.io.InputStream
import java.net.HttpURLConnection
import java.net.URI
import java.net.URLDecoder
import java.util.zip.ZipInputStream

/**
 * Opening a model from a model site's link (mobile_app.md, rule 10; decision of the spec team, 09-11-2026, as
 * Tisma Slicer does): `tisma://open?file=<encoded https URL>` or `prusaslicer://open?file=…` (Printables' «Open in
 * PrusaSlicer» button). Only https, only the sites listed, at most 200 MB, only STL, OBJ, 3MF, STEP or a ZIP with
 * one of them. Own design over that decision.
 */
object SiteLink {
    val SCHEMES = setOf("tisma", "prusaslicer")
    val HOSTS = setOf("printables.com", "media.printables.com", "files.printables.com", "thingiverse.com", "cdn.thingiverse.com")
    val EXTENSIONS = setOf("stl", "obj", "3mf", "step", "stp", "zip")
    const val MAX_BYTES = 200L * 1024 * 1024

    /** What a link asks to open: the file's address, its name and the site, for «Open <file> from <site>?». */
    data class Request(val url: String, val fileName: String, val site: String)

    sealed interface Parsed {
        data class Ok(val request: Request) : Parsed
        /** Why the link is refused, in a word the screen turns into a sentence. */
        data class Refused(val reason: Reason) : Parsed
    }

    enum class Reason { NOT_A_LINK, NOT_HTTPS, SITE, KIND }

    fun parse(link: String): Parsed {
        val u = runCatching { URI(link) }.getOrNull() ?: return Parsed.Refused(Reason.NOT_A_LINK)
        if (u.scheme?.lowercase() !in SCHEMES || u.host?.lowercase() != "open") return Parsed.Refused(Reason.NOT_A_LINK)
        val file = u.rawQuery?.split('&')?.firstOrNull { it.startsWith("file=") }?.substringAfter('=')
            ?.let { runCatching { URLDecoder.decode(it, "UTF-8") }.getOrNull() } ?: return Parsed.Refused(Reason.NOT_A_LINK)
        val target = runCatching { URI(file) }.getOrNull() ?: return Parsed.Refused(Reason.NOT_A_LINK)
        if (target.scheme?.lowercase() != "https") return Parsed.Refused(Reason.NOT_HTTPS)
        val host = target.host?.lowercase() ?: return Parsed.Refused(Reason.NOT_A_LINK)
        if (!allowedHost(host)) return Parsed.Refused(Reason.SITE)
        val name = target.path.orEmpty().substringAfterLast('/').let { runCatching { URLDecoder.decode(it, "UTF-8") }.getOrDefault(it) }
        if (name.substringAfterLast('.', "").lowercase() !in EXTENSIONS) return Parsed.Refused(Reason.KIND)
        return Parsed.Ok(Request(file, safeName(name), host.removePrefix("www.")))
    }

    /** The listed sites, and `www.` before them. */
    fun allowedHost(host: String) = host.lowercase().removePrefix("www.") in HOSTS

    fun safeName(name: String) = name.replace(Regex("[^A-Za-z0-9._ -]"), "_").ifBlank { "model.stl" }

    class DownloadException(val reason: Reason?, message: String) : IOException(message)

    /**
     * Downloads [request] into [dir]: https only (redirects must stay on https and on a listed site), at most
     * [MAX_BYTES]. A ZIP gives its first STL, OBJ, 3MF or STEP. Returns the model file.
     */
    fun download(request: Request, dir: File, open: (String) -> HttpURLConnection = { java.net.URL(it).openConnection() as HttpURLConnection }): File {
        dir.deleteRecursively(); dir.mkdirs()
        val c = open(request.url)
        c.instanceFollowRedirects = true
        c.connectTimeout = 15_000; c.readTimeout = 60_000
        try {
            val code = c.responseCode
            val final = c.url
            if (final.protocol != "https") throw DownloadException(Reason.NOT_HTTPS, "Not https")
            if (!allowedHost(final.host)) throw DownloadException(Reason.SITE, "Redirected to ${final.host}")
            if (code !in 200..299) throw DownloadException(null, "HTTP $code")
            if (c.contentLengthLong > MAX_BYTES) throw DownloadException(null, "Larger than 200 MB")
            val out = File(dir, request.fileName)
            c.inputStream.use { copyCapped(it, out) }
            return if (out.extension.equals("zip", true)) unzipModel(out, dir) else out
        } finally {
            c.disconnect()
        }
    }

    private fun copyCapped(input: InputStream, out: File) {
        var total = 0L
        out.outputStream().use { o ->
            val buf = ByteArray(64 * 1024)
            while (true) {
                val n = input.read(buf)
                if (n < 0) break
                total += n
                if (total > MAX_BYTES) { o.close(); out.delete(); throw DownloadException(null, "Larger than 200 MB") }
                o.write(buf, 0, n)
            }
        }
    }

    /** The first model in a ZIP, by its name only (no folders are made from it), also capped at 200 MB. */
    fun unzipModel(zip: File, dir: File): File {
        ZipInputStream(zip.inputStream().buffered()).use { z ->
            while (true) {
                val e = z.nextEntry ?: break
                if (e.isDirectory) continue
                val name = e.name.substringAfterLast('/')
                if (name.substringAfterLast('.', "").lowercase() !in setOf("stl", "obj", "3mf", "step", "stp")) continue
                val out = File(dir, safeName(name))
                copyCapped(z, out)
                return out
            }
        }
        throw DownloadException(Reason.KIND, "The ZIP has no STL, OBJ, 3MF or STEP")
    }
}
