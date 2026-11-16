package net.tisma.app.p2p

import kotlinx.cinterop.BetaInteropApi
import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.addressOf
import kotlinx.cinterop.usePinned
import net.tisma.app.updates.Ed25519
import net.tisma.cloud.CloudClient
import platform.Foundation.NSData
import platform.Foundation.NSDocumentDirectory
import platform.Foundation.NSFileHandle
import platform.Foundation.NSFileManager
import platform.Foundation.NSSearchPathForDirectoriesInDomains
import platform.Foundation.NSUserDefaults
import platform.Foundation.NSUserDomainMask
import platform.Foundation.create
import platform.Foundation.dataWithContentsOfFile
import platform.Foundation.fileHandleForWritingAtPath
import platform.Foundation.writeToFile
import platform.posix.memcpy

/** One model of the shared library as the SwiftUI list shows it. */
data class SharedModel(val id: String, val name: String, val author: String, val license: String, val checked: Boolean)

/**
 * «Abrir en la placa» (rule 8 octies): the checked file to put on the plate ([path]), or why not ([notice], one of
 * NOT_YET, BLOCKED, BUSY, SIGN_IN, DISCARDED, NOTHING_TO_OPEN, FAILED, as on Android).
 */
data class OpenResult(val path: String?, val name: String?, val notice: String?)

/**
 * The shared library for the iPhone app (p2p_library.md, rule 6), with the same fetch as Android ([LibraryFetch]).
 * The signatures are checked by CryptoKit: Swift hands its check over with [useEd25519] at start.
 */
@OptIn(ExperimentalForeignApi::class, BetaInteropApi::class)
object LibraryBridge {
    private var last: LibraryFetch.Result? = null

    /** Rule 3 ter bis: the last index offered was larger than this version takes (the app asks to be updated). */
    var tooLarge: Boolean = false
        private set

    /** Swift's Ed25519 check (CryptoKit's Curve25519.Signing.PublicKey.isValidSignature). */
    fun useEd25519(verify: (publicKey: NSData, message: NSData, signature: NSData) -> Boolean) {
        Ed25519.verifier = { k, m, s -> verify(k.toNSData(), m.toNSData(), s.toNSData()) }
    }

    /**
     * Fetches the key, list and index from [server] (kept ones without an answer). [projectKeyHex] is the project's
     * key (empty until the owner's key exists: then nothing is accepted). Gives null, or why it could not.
     */
    fun refresh(server: String, projectKeyHex: String): String? {
        val key = hexBytes(projectKeyHex)
        val client = CloudClient(server)
        val source = object : LibraryServer {
            override fun file(name: String) = client.libraryFile(name)
            override fun report(model: String?, sha256: String?, reason: String) = client.libraryReport(model, sha256, reason)
            override val origin: String get() = client.serverUrl
        }
        return runCatching { LibraryFetch(key, source, FileStore).refresh() }
            .also { tooLarge = it.exceptionOrNull() is LibraryIndex.TooLarge }
            .onSuccess { r -> last = LibraryFetch.Result(r.list ?: last?.list, r.index ?: last?.index) }
            .exceptionOrNull()?.let { it.message ?: "?" }
    }

    /** The models that match [query] (name, tag, author, license; accents ignored), blocked ones left out. */
    fun search(query: String): List<SharedModel> {
        val r = last ?: return emptyList()
        return r.index?.search(query, r.list).orEmpty().map { SharedModel(it.id, it.name, it.author, it.license, it.checked) }
    }

    /**
     * Rule 8 octies, as Android does it: the model's project or model file from the seeder at [server], with a
     * ticket from the person's session ([token]; empty: none), checked piece by piece and whole, kept in
     * Documents/library/files under its SHA-256. [progress] gets 0–100. Runs off the main thread.
     */
    fun openOnPlate(server: String, token: String, modelId: String, progress: (Int) -> Unit): OpenResult {
        val m = last?.index?.models?.firstOrNull { it.id == modelId } ?: return OpenResult(null, null, "FAILED")
        val file = LibraryOpen.fileToOpen(m) ?: return OpenResult(null, null, "NOTHING_TO_OPEN")
        val root = m.pieceRoots[file.sha256.lowercase()] ?: return OpenResult(null, null, "DISCARDED")
        if (token.isEmpty()) return OpenResult(null, null, "SIGN_IN")
        val sha = file.sha256.lowercase()
        val parts = DirPieces("${FileStore.folder}/downloads/$sha")
        val r = runCatching {
            SeedDownload(SeedClient(server, net.tisma.cloud.PlatformHttp)) { CloudClient(server, token).libraryTicket() }
                .download(file, root, parts) { done, of -> progress(if (of == 0) 100 else done * 100 / of) }
        }
        val result = r.getOrElse {
            return OpenResult(null, null, if ((it as? net.tisma.cloud.CloudException)?.status == 401) "SIGN_IN" else "FAILED")
        }
        return when (result) {
            is SeedDownload.Result.Done -> {
                val ext = file.path.substringAfterLast('.', "3mf").lowercase()
                val out = "${FileStore.folder}/files/$sha.$ext"
                if (!parts.assemble(SeedClient.pieceCount(file.size), out)) OpenResult(null, null, "FAILED")
                else OpenResult(out, LibraryOpen.name(file), null)
            }
            is SeedDownload.Result.NotYet -> OpenResult(null, null, "NOT_YET")
            is SeedDownload.Result.Blocked -> OpenResult(null, null, "BLOCKED")
            is SeedDownload.Result.Busy -> OpenResult(null, null, "BUSY")
            is SeedDownload.Result.Unauthorized -> OpenResult(null, null, "SIGN_IN")
            else -> OpenResult(null, null, "DISCARDED")
        }
    }

    /** A download's pieces as files in [dir], to resume after a cut (rule 8 octies). */
    private class DirPieces(private val dir: String) : PieceStore {
        private val fm get() = NSFileManager.defaultManager
        override fun has(n: Int) = fm.fileExistsAtPath("$dir/$n")
        override fun write(n: Int, bytes: ByteArray) {
            fm.createDirectoryAtPath(dir, true, null, null)
            bytes.toNSData().writeToFile("$dir/$n", atomically = true)
        }
        override fun read(n: Int): ByteArray = NSData.dataWithContentsOfFile("$dir/$n")?.toByteArray() ?: ByteArray(0)
        override fun clear() { fm.removeItemAtPath(dir, null) }
        /** The pieces in order into [out], one at a time; then the pieces go. */
        fun assemble(count: Int, out: String): Boolean {
            val folder = out.substringBeforeLast('/')
            fm.createDirectoryAtPath(folder, true, null, null)
            if (!fm.createFileAtPath(out, null, null)) return false
            val h = NSFileHandle.fileHandleForWritingAtPath(out) ?: return false
            for (n in 0 until count) {
                val piece = NSData.dataWithContentsOfFile("$dir/$n") ?: run { h.closeAndReturnError(null); return false }
                if (!h.writeData(piece, null)) { h.closeAndReturnError(null); return false }
            }
            h.closeAndReturnError(null)
            clear()
            return true
        }
    }

    private fun hexBytes(h: String): ByteArray {
        val t = h.trim()
        if (t.length % 2 != 0 || t.any { it !in "0123456789abcdefABCDEF" }) return ByteArray(0)
        return ByteArray(t.length / 2) { t.substring(2 * it, 2 * it + 2).toInt(16).toByte() }
    }

    /** Kept pieces in the app's Documents/library folder; sequences and server in the app's defaults. */
    private object FileStore : LibraryStore {
        val folder: String get() = dir
        private val dir: String by lazy {
            val docs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, true).first() as String
            "$docs/library".also { NSFileManager.defaultManager.createDirectoryAtPath(it, true, null, null) }
        }
        private val defaults get() = NSUserDefaults.standardUserDefaults

        override fun read(name: String): ByteArray? = NSData.dataWithContentsOfFile("$dir/$name")?.toByteArray()
        override fun write(name: String, bytes: ByteArray) { bytes.toNSData().writeToFile("$dir/$name", atomically = true) }
        override fun delete(name: String) { NSFileManager.defaultManager.removeItemAtPath("$dir/$name", null) }
        override var listSequence: Long
            get() = if (defaults.objectForKey("p2p.list") == null) -1L else defaults.integerForKey("p2p.list")
            set(v) { defaults.setInteger(v, "p2p.list") }
        override var indexSequence: Long
            get() = if (defaults.objectForKey("p2p.index") == null) -1L else defaults.integerForKey("p2p.index")
            set(v) { defaults.setInteger(v, "p2p.index") }
        override var origin: String?
            get() = defaults.stringForKey("p2p.origin") ?: ""
            set(v) { defaults.setObject(v, "p2p.origin") }
    }

    private fun ByteArray.toNSData(): NSData = if (isEmpty()) NSData() else usePinned { p ->
        NSData.create(bytes = p.addressOf(0), length = size.toULong())
    }

    private fun NSData.toByteArray(): ByteArray {
        val n = length.toInt()
        if (n == 0) return ByteArray(0)
        return ByteArray(n).also { b -> b.usePinned { p -> memcpy(p.addressOf(0), bytes, length) } }
    }
}
