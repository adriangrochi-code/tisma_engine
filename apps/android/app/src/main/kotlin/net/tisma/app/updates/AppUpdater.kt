package net.tisma.app.updates

import net.tisma.cloud.Http
import net.tisma.cloud.HttpRequest
import java.io.File
import java.io.IOException
import java.io.RandomAccessFile
import java.security.MessageDigest
import java.util.Base64

/**
 * The phone's updater for an APK installed by hand (app_updates.md, rules 1 to
 * 5, 11 and 13): it reads the signed list (GitHub first, Tisma Cloud's
 * `/v1/updates` if GitHub does not answer, AU12), offers the newer release,
 * downloads it in the background and goes on from where a cut download
 * stopped (AU4), and checks the package's SHA-256 and Ed25519 signature before
 * Android is asked to install it (AU2, AU3). From a store, the store updates
 * the app and this stays off.
 */
class AppUpdater(
    private val http: Http,
    private val publicKey: ByteArray,
    private val listUrls: List<String>,
    private val dir: File,
) {
    class UpdateException(message: String) : IOException(message)

    /** Reads the list from the first address that answers with a valid signed one. */
    fun releases(): List<Updates.Release> {
        var last: Exception? = null
        for (url in listUrls) {
            try {
                val json = get(url)
                val sig = get("$url.sig")
                return Updates.read(json, sig, publicKey)
            } catch (e: IOException) {
                last = e
            } catch (e: Updates.BadList) {
                last = e
            }
        }
        throw UpdateException(last?.message ?: "No release list")
    }

    private fun get(url: String): ByteArray {
        val r = http.send(HttpRequest("GET", url, mapOf("Accept" to "*/*"), timeoutMs = 30_000))
        r.use {
            if (!it.ok) throw IOException("HTTP ${it.status}")
            return it.body.readBytes()
        }
    }

    /**
     * Downloads [offer]'s APK into [dir], going on from a part already there,
     * then checks it. Returns the checked file; a damaged one is deleted (rule 5).
     */
    fun download(offer: Updates.Offer, onProgress: (Int) -> Unit, cancelled: () -> Boolean): File {
        val p = offer.pkg
        if (!p.url.startsWith("https://") && !p.url.startsWith("http://127.0.0.1")) throw UpdateException("Not an https address")
        dir.mkdirs()
        val name = "tisma-" + offer.release.version.replace(Regex("[^A-Za-z0-9.-]"), "_") + ".apk"
        val done = File(dir, name)
        if (done.isFile && check(done, p)) return done
        val part = File(dir, "$name.part")
        // Old downloads of other versions go.
        dir.listFiles()?.filter { it != part && it != done }?.forEach { it.delete() }
        var have = part.length()
        if (p.size in 1 until have) { part.delete(); have = 0 }
        val headers = mutableMapOf("Accept" to "*/*")
        if (have > 0) headers["Range"] = "bytes=$have-"
        val r = http.send(HttpRequest("GET", p.url, headers, timeoutMs = 60_000))
        r.use { resp ->
            if (resp.status == 416 && have == p.size) {
                // Already whole.
            } else {
                if (!resp.ok) throw UpdateException("HTTP ${resp.status}")
                // 206: the rest; 200: the server sent it all again.
                val append = resp.status == 206 && have > 0
                if (!append) have = 0
                RandomAccessFile(part, "rw").use { f ->
                    f.setLength(have)
                    f.seek(have)
                    val buf = ByteArray(64 * 1024)
                    while (true) {
                        if (cancelled()) throw UpdateException("Cancelled")
                        val n = resp.body.read(buf)
                        if (n < 0) break
                        // More than the list says is not the package: stop before it fills the phone.
                        if (p.size > 0 && have + n > p.size) throw UpdateException("damaged")
                        f.write(buf, 0, n)
                        have += n
                        if (p.size > 0) onProgress((100 * have / p.size).toInt().coerceIn(0, 100))
                    }
                }
            }
        }
        if (!check(part, p)) {
            part.delete()
            throw UpdateException("damaged")
        }
        if (!part.renameTo(done)) throw UpdateException("Cannot keep the download")
        return done
    }

    /** Size, SHA-256 and the package's own Ed25519 signature (rule 5). */
    fun check(f: File, p: Updates.Package): Boolean {
        if (p.size > 0 && f.length() != p.size) return false
        val sha = MessageDigest.getInstance("SHA-256")
        f.inputStream().use { i ->
            val buf = ByteArray(64 * 1024)
            while (true) { val n = i.read(buf); if (n < 0) break; sha.update(buf, 0, n) }
        }
        // The list's hex may come in either case.
        if (!sha.digest().joinToString("") { "%02x".format(it) }.equals(p.sha256.trim(), ignoreCase = true)) return false
        val sig = runCatching { Base64.getDecoder().decode(p.signature) }.getOrNull() ?: return false
        return f.inputStream().buffered().use { Ed25519.verify(publicKey, it, sig) }
    }

    companion object {
        /** Where the list lives (rule 13), and Tisma Cloud's copy as the second address. */
        const val GITHUB_LIST = "https://github.com/adriangrochi-code/tisma_engine/releases/download/updates/updates.json"
        fun cloudList(server: String) = server.trimEnd('/') + "/v1/updates"

        fun hexKey(s: String): ByteArray? =
            s.trim().takeIf { it.length == 64 }?.let { h -> runCatching { ByteArray(32) { h.substring(2 * it, 2 * it + 2).toInt(16).toByte() } }.getOrNull() }
    }
}
