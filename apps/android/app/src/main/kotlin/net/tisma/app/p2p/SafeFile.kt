package net.tisma.app.p2p

import java.io.File
import java.util.zip.ZipFile

/**
 * Whether a file may be opened or shared (p2p_library.md, rule 3 octies): only the library's kinds, each known by
 * its content and not by its name; a 3MF or ZIP that writes nowhere outside itself, has no links, unpacks to at
 * most ten times its size and 2 GB, and carries no programs. Own design over that rule.
 */
object SafeFile {
    enum class Kind { THREE_MF, ZIP, STL, OBJ, STEP, JSON, PNG, JPEG, WEBP }

    /** Why a file is not opened, for the screen's sentence. */
    enum class Problem { EMPTY, NOT_A_KIND, PROGRAM, DAMAGED, OUTSIDE_PATH, LINK, TOO_BIG }

    sealed interface Verdict {
        data class Safe(val kind: Kind, val warnings: List<String> = emptyList()) : Verdict
        data class Unsafe(val problem: Problem, val detail: String = "") : Verdict
    }

    const val MAX_UNPACKED = 2L * 1024 * 1024 * 1024

    /** Programs and macros that never travel in a model (rule 3 octies). */
    private val PROGRAMS = setOf("exe", "dll", "so", "dylib", "bat", "cmd", "com", "sh", "bash", "ps1", "vbs", "js", "jar",
        "apk", "dex", "py", "pl", "rb", "msi", "scr", "lnk", "app", "docm", "xlsm", "pptm", "dotm", "xltm", "elf")

    fun check(f: File): Verdict {
        if (!f.isFile || f.length() == 0L) return Verdict.Unsafe(Problem.EMPTY)
        val head = f.inputStream().use { i -> ByteArray(minOf(f.length(), 512L).toInt()).also { var o = 0; while (o < it.size) { val n = i.read(it, o, it.size - o); if (n < 0) break; o += n } } }
        fun starts(vararg b: Int) = head.size >= b.size && b.indices.all { head[it] == b[it].toByte() }
        val text = String(head, Charsets.ISO_8859_1)
        return when {
            starts(0x50, 0x4B, 0x03, 0x04) -> zip(f)
            starts(0x89, 0x50, 0x4E, 0x47) -> Verdict.Safe(Kind.PNG)
            starts(0xFF, 0xD8, 0xFF) -> Verdict.Safe(Kind.JPEG)
            head.size >= 12 && text.startsWith("RIFF") && text.substring(8, 12) == "WEBP" -> Verdict.Safe(Kind.WEBP)
            text.startsWith("ISO-10303-21") -> Verdict.Safe(Kind.STEP)
            binaryStl(f) -> Verdict.Safe(Kind.STL)
            text.trimStart().startsWith("solid") && isText(head) -> Verdict.Safe(Kind.STL)
            text.trimStart().startsWith("{") && isText(head) -> Verdict.Safe(Kind.JSON)
            isText(head) && Regex("(?m)^\\s*(v|vn|vt|f|o|g|#|mtllib|usemtl|s)\\s").containsMatchIn(text) -> Verdict.Safe(Kind.OBJ)
            starts(0x7F, 0x45, 0x4C, 0x46) || starts(0x4D, 0x5A) || text.startsWith("#!") -> Verdict.Unsafe(Problem.PROGRAM)
            else -> Verdict.Unsafe(Problem.NOT_A_KIND)
        }
    }

    private fun isText(b: ByteArray) = b.none { it.toInt() == 0 }

    /** A binary STL: 80 bytes, a count, 50 bytes a triangle, and nothing else. */
    private fun binaryStl(f: File): Boolean {
        if (f.length() < 84) return false
        val h = f.inputStream().use { i -> ByteArray(84).also { var o = 0; while (o < 84) { val n = i.read(it, o, 84 - o); if (n < 0) break; o += n } } }
        val n = (h[80].toLong() and 0xFF) or ((h[81].toLong() and 0xFF) shl 8) or ((h[82].toLong() and 0xFF) shl 16) or ((h[83].toLong() and 0xFF) shl 24)
        return 84 + 50 * n == f.length()
    }

    private fun zip(f: File): Verdict {
        if (hasLink(f)) return Verdict.Unsafe(Problem.LINK)
        val z = runCatching { ZipFile(f) }.getOrNull() ?: return Verdict.Unsafe(Problem.DAMAGED)
        z.use {
            val cap = minOf(10 * f.length(), MAX_UNPACKED)
            var total = 0L
            var model = false
            val warnings = mutableListOf<String>()
            for (e in z.entries()) {
                val name = e.name.replace('\\', '/')
                if (name.startsWith("/") || Regex("^[A-Za-z]:").containsMatchIn(name) || name.split('/').any { it == ".." }) {
                    return Verdict.Unsafe(Problem.OUTSIDE_PATH, name)
                }
                val ext = name.substringAfterLast('/').substringAfterLast('.', "").lowercase()
                if (ext in PROGRAMS) return Verdict.Unsafe(Problem.PROGRAM, name)
                if (e.size > 0) total += e.size else if (!e.isDirectory) total += e.compressedSize
                if (total > cap) return Verdict.Unsafe(Problem.TOO_BIG)
                if (name.startsWith("3D/") && name.endsWith(".model")) model = true
                // Rule 3 octies: a 3MF's own G-code or post-processing scripts are taken out, with a notice.
                if (ext in LibraryIndex.GCODE || name.contains("post_process", ignoreCase = true)) warnings += name
            }
            return Verdict.Safe(if (model) Kind.THREE_MF else Kind.ZIP, warnings)
        }
    }

    /**
     * Whether an entry is a Unix link: the central directory's «version made by» says Unix (3) and its external
     * attributes carry the link mode (0xA000 in the high 16 bits). From the ZIP format's public description
     * (PKWARE APPNOTE, «central directory structure»); java.util.zip does not expose these attributes.
     */
    internal fun hasLink(f: File): Boolean = runCatching {
        java.io.RandomAccessFile(f, "r").use { r ->
            val len = r.length()
            val tail = minOf(len, 65_557L).toInt()
            val buf = ByteArray(tail)
            r.seek(len - tail); r.readFully(buf)
            fun u16(b: ByteArray, i: Int) = (b[i].toInt() and 0xFF) or ((b[i + 1].toInt() and 0xFF) shl 8)
            fun u32(b: ByteArray, i: Int) = (u16(b, i).toLong()) or (u16(b, i + 2).toLong() shl 16)
            var eocd = -1
            for (i in tail - 22 downTo 0) if (u32(buf, i) == 0x06054b50L) { eocd = i; break }
            if (eocd < 0) return@use false
            val count = u16(buf, eocd + 10)
            val size = u32(buf, eocd + 12)
            val offset = u32(buf, eocd + 16)
            if (offset + size > len || size > 64L * 1024 * 1024) return@use false
            val cd = ByteArray(size.toInt())
            r.seek(offset); r.readFully(cd)
            var p = 0
            repeat(count) {
                if (p + 46 > cd.size || u32(cd, p) != 0x02014b50L) return@use false
                val madeBy = u16(cd, p + 4) shr 8
                val external = u32(cd, p + 38)
                if (madeBy == 3 && ((external shr 16) and 0xF000L) == 0xA000L) return@use true
                p += 46 + u16(cd, p + 28) + u16(cd, p + 30) + u16(cd, p + 32)
            }
            false
        }
    }.getOrDefault(false)
}
