package net.tisma.printers

import net.tisma.mockprinters.MockFlashforge
import org.junit.After
import org.junit.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

/** Flashforge on the local network against the simulator (printer_connections.md, «Flashforge en red local», FF1). */
class FlashforgeTest {
    private val modern = MockFlashforge().start()
    private val legacy = MockFlashforge(serial = "SNLEGACY01", modern = false).start()

    @After
    fun stop() { modern.stop(); legacy.stop() }

    private fun connect(code: String = modern.checkCode) =
        Flashforge.of(modern.url, modern.serial, code, net.tisma.cloud.UrlConnectionHttp)

    @Test
    fun `FF1_1 discovery reads a modern answer with its serial and HTTP port and an older one without serial`() {
        val m = assertNotNull(Flashforge.parseAnswer(modern.discoveryAnswer()))
        assertTrue(m.modern)
        assertEquals("Adventurer 5M Pro", m.name)
        assertEquals("SNTEST0001", m.serial)
        assertEquals(modern.httpPort, m.httpPort)
        assertEquals(modern.tcpPort, m.tcpPort)
        val l = assertNotNull(Flashforge.parseAnswer(legacy.discoveryAnswer("Adventurer 4")))
        assertTrue(!l.modern)
        assertEquals("Adventurer 4", l.name)
        assertEquals("", l.serial)
        assertEquals(legacy.tcpPort, l.tcpPort)
        // Its serial comes from M115.
        val c = Flashforge.of(legacy.url, "", "", net.tisma.cloud.UrlConnectionHttp) as FlashforgeTcp
        c.test()
        assertEquals("SNLEGACY01", c.serial)
        // A 276-byte answer (firmware 3.x) is modern too; other sizes are not Flashforge's.
        assertTrue(Flashforge.parseAnswer(modern.discoveryAnswer().copyOf(276))!!.modern)
        assertEquals(null, Flashforge.parseAnswer(ByteArray(100)))
    }

    @Test
    fun `FF1_1 the probe lists both on the network, the modern one with serial and HTTP port`() {
        val targets = listOf(java.net.InetSocketAddress("127.0.0.1", modern.answerDiscovery()),
            java.net.InetSocketAddress("127.0.0.1", legacy.answerDiscovery("Adventurer 4")))
        val found = FlashforgeProbe.probe(targets, 1_500)
        // Both answer from the loopback: one entry per address, so each printer is probed on its own here.
        assertEquals(1, found.size)
        val m = FlashforgeProbe.probe(targets.take(1), 1_500).single()
        assertEquals(PrinterKind.FLASHFORGE, m.kind)
        assertEquals("SNTEST0001", m.serial)
        assertEquals(modern.httpPort, m.port)
        assertEquals("flashforge://127.0.0.1", m.url)
        val l = FlashforgeProbe.probe(targets.drop(1), 1_500).single()
        assertEquals("Adventurer 4", l.name)
        assertEquals("", l.serial)
    }

    @Test
    fun `FF1_2 a wrong check code is said once, as credentials`() {
        val e = assertFailsWith<PrinterException> { connect("ffffffff").status() }
        assertEquals(PrinterException.Kind.AUTH, e.kind)
        assertEquals(PrinterException.Reason.FF_AUTH, e.reason)
        assertEquals(1, modern.requests.count { it.first == "/detail" }, "one request, no retry loop")
    }

    @Test
    fun `FF1_3 upload with print now and levelling sends the file as gcodeFile with its size in the headers`() {
        val gcode = "G28\nG1 X10 Y10 E1\n".toByteArray()
        connect().upload("cube.gcode", gcode.size.toLong(), start = true, PrintOptions(bedLeveling = true)) { net.tisma.io.BytesInput(gcode) }
        val u = modern.uploads.single()
        assertEquals("gcodeFile", u.field)
        assertEquals("cube.gcode", u.name)
        assertTrue(u.bytes.contentEquals(gcode))
        assertEquals(gcode.size.toString(), u.headers["filesize"])
        assertEquals("true", u.headers["printnow"])
        assertEquals("true", u.headers["levelingbeforeprint"])
        assertEquals(modern.serial, u.headers["serialnumber"])
        assertEquals(PrinterState.PRINTING, connect().status().state)
    }

    @Test
    fun `FF1_4 pause and cancel go as jobCtl_cmd and the state comes from detail`() {
        val c = connect() as FlashforgeHttp
        c.confirmMs = 3_000
        val gcode = "G28\n".toByteArray()
        c.upload("a.gcode", gcode.size.toLong(), start = true) { net.tisma.io.BytesInput(gcode) }
        c.pause()
        assertEquals(PrinterState.PAUSED, c.status().state)
        c.cancel()
        val s = c.status()
        assertEquals(PrinterState.READY, s.state)
        assertEquals(false, s.finished)
        val actions = modern.requests.filter { it.first == "/control" }.map {
            val p = it.second["payload"] as kotlinx.serialization.json.JsonObject
            (p["cmd"] as kotlinx.serialization.json.JsonPrimitive).content + ":" +
                ((p["args"] as kotlinx.serialization.json.JsonObject)["action"] as kotlinx.serialization.json.JsonPrimitive).content
        }
        assertEquals(listOf("jobCtl_cmd:pause", "jobCtl_cmd:cancel"), actions)
    }

    @Test
    fun `an action the printer ignores is not reported as done`() {
        val c = connect() as FlashforgeHttp
        c.confirmMs = 1_000
        // Nothing printing: «Success» to the pause, but /detail still says ready.
        val e = assertFailsWith<PrinterException> { c.pause() }
        assertEquals(PrinterException.Kind.CONFLICT, e.kind)
    }

    @Test
    fun `FF1_5 an older one whose control session is taken sends nothing else`() {
        legacy.controlTaken = true
        val c = Flashforge.of(legacy.url, "", "", net.tisma.cloud.UrlConnectionHttp)
        val e = assertFailsWith<PrinterException> { c.resume() }
        assertEquals(PrinterException.Reason.FF_OTHER_PROGRAM, e.reason)
        assertTrue(legacy.tcpCommands.none { it.startsWith("M23") || it.startsWith("M24") }, "${legacy.tcpCommands}")
    }

    @Test
    fun `an older one reports its state and temperatures over TCP and releases the session`() {
        val c = Flashforge.of(legacy.url, "", "", net.tisma.cloud.UrlConnectionHttp)
        val s = c.status()
        assertEquals(PrinterState.READY, s.state)
        assertEquals(Temperature(17.9, 0.0), s.nozzle)
        assertEquals(Temperature(18.5, 0.0), s.bed)
        assertEquals("M602", legacy.tcpCommands.last())
        val e = assertFailsWith<PrinterException> { c.upload("a.gcode", 1, true) { net.tisma.io.BytesInput(byteArrayOf(1)) } }
        assertEquals(PrinterException.Reason.FF_NO_UPLOAD, e.reason)
    }

    @Test
    fun `FF1_6 a closed port says it could not connect, with the address, within the timeout`() {
        val closed = java.net.ServerSocket(0).use { it.localPort }
        val t0 = System.currentTimeMillis()
        val e = assertFailsWith<PrinterException> {
            Flashforge.of("flashforge://127.0.0.1?http=$closed&tcp=$closed", "SN", "code", net.tisma.cloud.UrlConnectionHttp).status()
        }
        assertEquals(PrinterException.Kind.NETWORK, e.kind)
        assertEquals(PrinterException.Reason.FF_UNREACHABLE_LAN, e.reason)
        assertTrue("127.0.0.1" in e.detail)
        assertTrue(System.currentTimeMillis() - t0 < 15_000)
        val l = assertFailsWith<PrinterException> { Flashforge.of("flashforge://127.0.0.1?tcp=$closed", "", "", net.tisma.cloud.UrlConnectionHttp).status() }
        assertEquals(PrinterException.Reason.FF_UNREACHABLE, l.reason)
    }

    @Test
    fun `the address is the host`() {
        assertEquals("flashforge://192.168.1.40", PrinterConfig.normalizeUrl("192.168.1.40", PrinterKind.FLASHFORGE))
        assertEquals("flashforge://192.168.1.40", PrinterConfig.normalizeUrl("http://192.168.1.40", PrinterKind.FLASHFORGE))
    }
}
