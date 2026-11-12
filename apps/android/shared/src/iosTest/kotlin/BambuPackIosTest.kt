import net.tisma.io.BytesInput
import net.tisma.io.BytesOutput
import net.tisma.io.Md5
import net.tisma.io.copyAll
import net.tisma.io.hex
import net.tisma.printers.packForBambu
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** On the iPhone, the Bambu project is packed through files: a ZIP with the G-code and its MD5. */
class BambuPackIosTest {
    @Test
    fun theProjectHoldsTheGcodeAndItsMd5() {
        val gcode = ByteArray(200_003) { (it % 97).toByte() }
        val p = packForBambu(BytesInput(gcode))
        try {
            assertEquals(hex(Md5.digest(gcode)), p.md5)
            val zip = BytesOutput().also { out -> p.open().use { it.copyAll(out) } }.toByteArray()
            assertEquals(p.length, zip.size.toLong())
            assertTrue(zip[0] == 'P'.code.toByte() && zip[1] == 'K'.code.toByte())
            assertTrue(zip.decodeToString().contains("Metadata/plate_1.gcode.md5"))
            assertTrue(zip.decodeToString().contains(p.md5))
        } finally {
            p.delete()
        }
    }
}
