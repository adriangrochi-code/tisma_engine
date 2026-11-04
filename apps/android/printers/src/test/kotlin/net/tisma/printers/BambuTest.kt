package net.tisma.printers

import net.tisma.mockprinters.MockBambu
import org.junit.After
import org.junit.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

/** Bambu Lab local mode against the simulator (BL1, BL2, BL3). */
class BambuTest {
    private val printer = MockBambu().start()

    @After
    fun stop() = printer.stop()

    private fun connect(code: String = printer.accessCode, pinned: String? = null) =
        BambuConnection.of(printer.url, code, pinned)

    @Test
    fun `BL1 state, upload as gcode 3mf with its MD5, project_file, pause, resume, stop`() {
        val c = connect()
        assertEquals("Bambu Lab ${printer.serial}", c.test())
        assertEquals(printer.serial, c.seenSerial)
        assertNotNull(c.seenFingerprint)
        assertEquals(PrinterState.READY, c.status().state)
        val gcode = "G28\nG1 X10 Y10 E1\n".toByteArray()
        c.upload("cube.gcode", gcode.size.toLong(), start = true) { gcode.inputStream() }
        val (stored, md5) = assertNotNull(printer.storedGcode("cube.gcode.3mf"))
        assertTrue(stored.contentEquals(gcode))
        assertEquals(BambuConnection.md5(gcode), md5)
        val pf = printer.commands.single { it["command"].toString() == "\"project_file\"" }
        assertEquals("\"ftp:///cube.gcode.3mf\"", pf["url"].toString())
        assertEquals("\"Metadata/plate_1.gcode\"", pf["param"].toString())
        Thread.sleep(200)
        val s = c.status()
        assertEquals(PrinterState.PRINTING, s.state)
        assertEquals(10.0, s.percent)
        assertEquals(600L, s.remainingS)
        assertEquals(Temperature(220.0, 220.0), s.nozzle)
        c.pause(); Thread.sleep(200)
        assertEquals(PrinterState.PAUSED, c.status().state)
        c.resume(); Thread.sleep(200)
        assertEquals(PrinterState.PRINTING, c.status().state)
        c.cancel(); Thread.sleep(200)
        assertEquals(PrinterState.READY, c.status().state)
        c.setTemperature(Heater.NOZZLE, 200.0); Thread.sleep(200)
        assertEquals(200.0, c.status().nozzle?.target)
        c.close()
    }

    @Test
    fun `BL2 a wrong access code is a clear authentication error`() {
        val e = assertFailsWith<PrinterException> { connect("00000000").test() }
        assertEquals(PrinterException.Kind.AUTH, e.kind)
        assertTrue("00000000" !in e.message.orEmpty())
    }

    @Test
    fun `BL3 a certificate other than the saved one does not connect`() {
        val first = connect().also { it.test(); it.close() }
        val pinned = assertNotNull(first.seenFingerprint)
        connect(pinned = pinned).also { assertEquals(PrinterState.READY, it.status().state); it.close() }
        val e = assertFailsWith<PrinterException> { connect(pinned = "00:" + pinned.substring(3)).test() }
        assertEquals(PrinterException.Kind.CERTIFICATE, e.kind)
    }
}
