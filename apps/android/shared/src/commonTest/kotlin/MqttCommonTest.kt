import net.tisma.io.ByteInput
import net.tisma.io.ByteOutput
import net.tisma.io.BytesInput
import net.tisma.io.BytesOutput
import net.tisma.io.Link
import net.tisma.printers.MqttClient
import net.tisma.printers.PrinterException
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

/** MQTT 3.1.1 (OASIS, 2014) over a link in memory, the same on Android and iOS. */
class MqttCommonTest {
    private class Memory(answer: ByteArray) : Link {
        val written = BytesOutput()
        override val input: ByteInput = BytesInput(answer)
        override val output: ByteOutput = written
        override fun close() {}
    }

    @Test
    fun connectSendsUserAndPasswordAndReadsTheAck() {
        val link = Memory(byteArrayOf(0x20, 2, 0, 0)) // CONNACK, accepted
        val c = MqttClient(link, "id", { _, _ -> })
        c.connect("bblp", "12345678")
        val p = link.written.toByteArray()
        assertEquals(0x10, p[0].toInt() and 0xFF)
        // Variable header (3.1.2): "MQTT", level 4, flags user+password+clean session, keep alive 60.
        assertContentEquals(byteArrayOf(0, 4, 'M'.code.toByte(), 'Q'.code.toByte(), 'T'.code.toByte(), 'T'.code.toByte(), 4, 0xC2.toByte(), 0, 60),
            p.copyOfRange(2, 12))
        assertTrue(p.decodeToString().contains("bblp") && p.decodeToString().contains("12345678"))
    }

    @Test
    fun aRefusedAccessCodeIsAnAuthError() {
        val e = assertFailsWith<PrinterException> { MqttClient(Memory(byteArrayOf(0x20, 2, 0, 5)), "id", { _, _ -> }).connect("bblp", "bad") }
        assertEquals(PrinterException.Kind.AUTH, e.kind)
    }

    @Test
    fun aPacketIsReadFromItsRemainingLength() {
        val body = ByteArray(200) { it.toByte() }
        val out = BytesOutput()
        MqttClient.writeTo(out, 0x30, body)
        val (type, read) = MqttClient.readFrom(BytesInput(out.toByteArray()))!!
        assertEquals(0x30, type)
        assertContentEquals(body, read)
    }
}
