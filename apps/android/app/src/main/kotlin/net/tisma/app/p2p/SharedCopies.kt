package net.tisma.app.p2p

import java.io.File
import java.security.MessageDigest

/** The SHA-256 of a file, in lowercase hex (how the library names files). */
fun sha256(f: File): String {
    val md = MessageDigest.getInstance("SHA-256")
    f.inputStream().use { i -> val b = ByteArray(64 * 1024); while (true) { val n = i.read(b); if (n < 0) break; md.update(b, 0, n) } }
    return md.digest().joinToString("") { "%02x".format(it) }
}

/**
 * The shared library's files on this phone (p2p_library.md, rule 2): only what the person published or downloaded,
 * each named by its SHA-256. «Mi biblioteca» is not here and is never shared (rule 8).
 */
class SharedCopies(val dir: File) {
    data class Copy(val sha256: String, val file: File)

    fun all(): List<Copy> = dir.listFiles().orEmpty().filter { it.isFile }.map { Copy(it.nameWithoutExtension.lowercase(), it) }

    /** Keeps [file] under its SHA-256 (checked, not trusted from its name). */
    fun add(file: File, extension: String): Copy {
        dir.mkdirs()
        val sha = sha256(file)
        val target = File(dir, "$sha.$extension")
        file.copyTo(target, overwrite = true)
        return Copy(sha, target)
    }

    /** Rule 4: a blocked file is not kept: deleted, and returned so the person is told. */
    fun apply(list: BlockList): List<Copy> = all().filter { list.blocksFile(it.sha256) }.onEach { it.file.delete() }
}

