package net.tisma.app.p2p

import kotlinx.cinterop.BetaInteropApi
import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.addressOf
import kotlinx.cinterop.usePinned
import net.tisma.app.updates.Ed25519
import net.tisma.cloud.CloudClient
import platform.Foundation.NSData
import platform.Foundation.NSDocumentDirectory
import platform.Foundation.NSFileManager
import platform.Foundation.NSSearchPathForDirectoriesInDomains
import platform.Foundation.NSUserDefaults
import platform.Foundation.NSUserDomainMask
import platform.Foundation.create
import platform.Foundation.dataWithContentsOfFile
import platform.Foundation.writeToFile
import platform.posix.memcpy

/** One model of the shared library as the SwiftUI list shows it. */
data class SharedModel(val id: String, val name: String, val author: String, val license: String, val checked: Boolean)

/**
 * The shared library for the iPhone app (p2p_library.md, rule 6), with the same fetch as Android ([LibraryFetch]).
 * The signatures are checked by CryptoKit: Swift hands its check over with [useEd25519] at start.
 */
@OptIn(ExperimentalForeignApi::class, BetaInteropApi::class)
object LibraryBridge {
    private var last: LibraryFetch.Result? = null

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
            .onSuccess { r -> last = LibraryFetch.Result(r.list ?: last?.list, r.index ?: last?.index) }
            .exceptionOrNull()?.let { it.message ?: "?" }
    }

    /** The models that match [query] (name, tag, author, license; accents ignored), blocked ones left out. */
    fun search(query: String): List<SharedModel> {
        val r = last ?: return emptyList()
        return r.index?.search(query, r.list).orEmpty().map { SharedModel(it.id, it.name, it.author, it.license, it.checked) }
    }

    private fun hexBytes(h: String): ByteArray {
        val t = h.trim()
        if (t.length % 2 != 0 || t.any { it !in "0123456789abcdefABCDEF" }) return ByteArray(0)
        return ByteArray(t.length / 2) { t.substring(2 * it, 2 * it + 2).toInt(16).toByte() }
    }

    /** Kept pieces in the app's Documents/library folder; sequences and server in the app's defaults. */
    private object FileStore : LibraryStore {
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
