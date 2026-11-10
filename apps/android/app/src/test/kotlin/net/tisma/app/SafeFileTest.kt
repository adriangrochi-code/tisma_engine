package net.tisma.app

import net.tisma.app.p2p.SafeFile
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream
import kotlin.test.assertEquals
import kotlin.test.assertIs
import kotlin.test.assertTrue

/** p2p_library.md, rule 3 octies: a file is known by its content, and a 3MF or ZIP is checked before it is opened. */
class SafeFileTest {
    @get:Rule
    val tmp = TemporaryFolder()

    private fun zip(name: String, vararg entries: Pair<String, ByteArray>): File = tmp.newFile(name).also { f ->
        ZipOutputStream(f.outputStream()).use { z -> entries.forEach { (n, b) -> z.putNextEntry(ZipEntry(n)); z.write(b); z.closeEntry() } }
    }

    private fun kind(f: File) = (SafeFile.check(f) as SafeFile.Verdict.Safe).kind
    private fun problem(f: File) = (SafeFile.check(f) as SafeFile.Verdict.Unsafe).problem

    @Test
    fun `kinds are known by their content, not their name`() {
        assertEquals(SafeFile.Kind.STL, kind(tmp.newFile("a.png").apply { writeText("solid x\nendsolid x\n") }))
        val bin = ByteArray(84 + 50).also { it[80] = 1 }
        assertEquals(SafeFile.Kind.STL, kind(tmp.newFile("b.bin").apply { writeBytes(bin) }))
        assertEquals(SafeFile.Kind.OBJ, kind(tmp.newFile("c.txt").apply { writeText("# cube\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n") }))
        assertEquals(SafeFile.Kind.PNG, kind(tmp.newFile("d.stl").apply { writeBytes(byteArrayOf(0x89.toByte(), 0x50, 0x4E, 0x47, 0x0D, 0x0A)) }))
        assertEquals(SafeFile.Kind.STEP, kind(tmp.newFile("e.step").apply { writeText("ISO-10303-21;\nHEADER;\n") }))
        assertEquals(SafeFile.Kind.THREE_MF, kind(zip("f.3mf", "3D/3dmodel.model" to "<model/>".toByteArray())))
        assertEquals(SafeFile.Kind.ZIP, kind(zip("g.zip", "readme.txt" to "hi".toByteArray())))
    }

    @Test
    fun `a program is refused even when renamed`() {
        assertEquals(SafeFile.Problem.PROGRAM, problem(tmp.newFile("model.stl").apply { writeBytes(byteArrayOf(0x7F, 0x45, 0x4C, 0x46, 2, 1, 1)) }))
        assertEquals(SafeFile.Problem.PROGRAM, problem(tmp.newFile("model.obj").apply { writeBytes(byteArrayOf(0x4D, 0x5A, 0, 0, 0)) }))
        assertEquals(SafeFile.Problem.PROGRAM, problem(tmp.newFile("model.3mf").apply { writeText("#!/bin/sh\nrm -rf x\n") }))
        assertEquals(SafeFile.Problem.PROGRAM, problem(zip("h.3mf", "3D/3dmodel.model" to "<m/>".toByteArray(), "Metadata/run.exe" to "MZ".toByteArray())))
        assertEquals(SafeFile.Problem.NOT_A_KIND, problem(tmp.newFile("x.stl").apply { writeBytes(byteArrayOf(1, 2, 0, 3)) }))
        // A script with a comment line is still a program, not an OBJ; text with only comments is no model.
        assertEquals(SafeFile.Problem.PROGRAM, problem(tmp.newFile("s.obj").apply { writeText("#!/bin/sh\n# cleanup\nrm -rf x\n") }))
        assertEquals(SafeFile.Problem.NOT_A_KIND, problem(tmp.newFile("c.obj").apply { writeText("# just a note\ng group\n") }))
        assertEquals(SafeFile.Problem.EMPTY, problem(tmp.newFile("empty.stl")))
    }

    @Test
    fun `a zip that writes outside itself, links or unpacks too much is refused`() {
        val outside = SafeFile.check(zip("i.zip", "../../etc/x" to "x".toByteArray()))
        assertIs<SafeFile.Verdict.Unsafe>(outside)
        assertEquals(SafeFile.Problem.OUTSIDE_PATH, outside.problem)
        assertEquals(SafeFile.Problem.OUTSIDE_PATH, problem(zip("j.zip", "/abs/x" to "x".toByteArray())))
        // A bomb: 4 MB of zeros packs to a few KB, far over ten times the file.
        assertEquals(SafeFile.Problem.TOO_BIG, problem(zip("k.zip", "zeros.bin" to ByteArray(4 shl 20))))
        assertEquals(SafeFile.Problem.DAMAGED, problem(tmp.newFile("l.zip").apply { writeBytes(byteArrayOf(0x50, 0x4B, 3, 4, 9, 9)) }))
        assertEquals(SafeFile.Problem.LINK, problem(withLink(zip("m.zip", "link" to "/etc/passwd".toByteArray()))))
    }

    @Test
    fun `a 3MF's own G-code or post-processing script is named so it can be taken out`() {
        val v = SafeFile.check(zip("n.3mf", "3D/3dmodel.model" to "<m/>".toByteArray(), "Metadata/plate_1.gcode" to "G1".toByteArray()))
        assertIs<SafeFile.Verdict.Safe>(v)
        assertTrue("Metadata/plate_1.gcode" in v.warnings)
    }

    /** Marks the first central-directory entry as made on Unix with the link mode (APPNOTE, external attributes). */
    private fun withLink(f: File): File {
        val b = f.readBytes()
        val cd = (0 until b.size - 4).first { b[it] == 0x50.toByte() && b[it + 1] == 0x4B.toByte() && b[it + 2] == 1.toByte() && b[it + 3] == 2.toByte() }
        b[cd + 5] = 3 // version made by: Unix
        b[cd + 38 + 2] = 0xFF.toByte(); b[cd + 38 + 3] = 0xA1.toByte() // mode 0xA1FF: a link, rwx
        f.writeBytes(b)
        return f
    }
}
