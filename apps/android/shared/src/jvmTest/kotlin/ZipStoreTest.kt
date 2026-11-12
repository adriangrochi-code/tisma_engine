import net.tisma.io.BytesInput
import net.tisma.io.BytesOutput
import net.tisma.io.Crc32
import net.tisma.io.Md5Digest
import net.tisma.io.StoredEntry
import net.tisma.io.hex
import net.tisma.io.writeStoredZip
import java.security.MessageDigest
import java.util.zip.CRC32
import java.util.zip.ZipInputStream
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals

/** Our ZIP writer, CRC-32 and MD5 in parts, checked against the JDK's. */
class ZipStoreTest {
    private val gcode = ByteArray(300_001) { (it * 7 % 251).toByte() }

    @Test
    fun crcAndMd5InPartsMatchTheJdk() {
        val crc = Crc32(); val md = Md5Digest()
        var i = 0
        while (i < gcode.size) { val n = minOf(4093, gcode.size - i); crc.update(gcode, i, n); md.update(gcode, i, n); i += n }
        assertEquals(CRC32().apply { update(gcode) }.value, crc.value)
        assertEquals(hex(MessageDigest.getInstance("MD5").digest(gcode)), hex(md.digest()))
        assertEquals("d41d8cd98f00b204e9800998ecf8427e", hex(Md5Digest().digest()))
    }

    @Test
    fun theJdkReadsOurStoredZip() {
        val sum = "0123456789abcdef0123456789abcdef".encodeToByteArray()
        fun entry(name: String, b: ByteArray) = StoredEntry(name, b.size.toLong(), Crc32().apply { update(b, 0, b.size) }.value) { BytesInput(b) }
        val out = BytesOutput()
        writeStoredZip(out, listOf(entry("Metadata/plate_1.gcode", gcode), entry("Metadata/plate_1.gcode.md5", sum)))
        val read = mutableMapOf<String, ByteArray>()
        ZipInputStream(out.toByteArray().inputStream()).use { z ->
            while (true) { val e = z.nextEntry ?: break; read[e.name] = z.readBytes() }
        }
        assertContentEquals(gcode, read["Metadata/plate_1.gcode"])
        assertContentEquals(sum, read["Metadata/plate_1.gcode.md5"])
        java.util.zip.ZipFile(java.io.File.createTempFile("zipstore", ".zip").apply { writeBytes(out.toByteArray()); deleteOnExit() }).use { zf ->
            assertEquals(2, zf.size())
            assertEquals(gcode.size.toLong(), zf.getEntry("Metadata/plate_1.gcode").size)
        }
    }
}
