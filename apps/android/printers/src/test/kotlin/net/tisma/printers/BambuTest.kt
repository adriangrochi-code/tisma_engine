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
    fun `a link that goes silent without closing is not shown as the last state for ever`() {
        val c = BambuConnection.of(printer.url, printer.accessCode, null, staleMs = 600)
        printer.gcodeState = "RUNNING"; printer.percent = 45
        assertEquals(PrinterState.PRINTING, c.status().state)
        // Wi-Fi lost: the socket stays open, nothing comes.
        printer.silent = true
        Thread.sleep(700)
        val e = assertFailsWith<PrinterException> { c.status() }
        assertEquals(PrinterException.Kind.NETWORK, e.kind)
        // Back: a new link, and the printer's own state, not the old 45 %.
        printer.silent = false
        printer.gcodeState = "IDLE"; printer.percent = 0
        assertEquals(PrinterState.READY, c.status().state)
    }

    @Test
    fun `a refused upload closes its data connection`() {
        val c = connect()
        printer.refuseStore = true
        val gcode = "G28\n".toByteArray()
        assertFailsWith<PrinterException> { c.upload("cube.gcode", gcode.size.toLong(), start = false) { gcode.inputStream() } }
        val deadline = System.currentTimeMillis() + 10_000
        while (printer.refusedDataClosed == null && System.currentTimeMillis() < deadline) Thread.sleep(20)
        assertEquals(true, printer.refusedDataClosed)
    }

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
        // gcode_macros.md, rule 12: by default the bed is levelled and the flow calibrated.
        assertEquals("true", pf["bed_levelling"].toString())
        assertEquals("true", pf["flow_cali"].toString())
        assertEquals("false", pf["vibration_cali"].toString())
        assertEquals("false", pf["timelapse"].toString())
        Thread.sleep(200)
        val s = c.status()
        assertEquals(PrinterState.PRINTING, s.state)
        assertEquals(10.0, s.percent)
        assertEquals(null, s.remainingS) // unit not documented: not shown
        assertEquals(Temperature(220.0, 220.0), s.nozzle)
        c.pause(); Thread.sleep(200)
        assertEquals(PrinterState.PAUSED, c.status().state)
        c.resume(); Thread.sleep(200)
        assertEquals(PrinterState.PRINTING, c.status().state)
        c.cancel(); Thread.sleep(200)
        assertEquals(PrinterState.READY, c.status().state)
        // The options chosen when sending go in the print command.
        c.upload("cube2.gcode", gcode.size.toLong(), start = true,
            options = PrintOptions(bedLeveling = false, flowCalibration = false, vibrationCalibration = true, timelapse = true)) { gcode.inputStream() }
        val deadline = System.currentTimeMillis() + 5_000
        while (printer.commands.count { it["command"].toString() == "\"project_file\"" } < 2 && System.currentTimeMillis() < deadline) Thread.sleep(20)
        val pf2 = printer.commands.last { it["command"].toString() == "\"project_file\"" }
        assertEquals("false", pf2["bed_levelling"].toString())
        assertEquals("false", pf2["flow_cali"].toString())
        assertEquals("true", pf2["vibration_cali"].toString())
        assertEquals("true", pf2["timelapse"].toString())
        c.cancel(); Thread.sleep(200)
        c.setTemperature(Heater.NOZZLE, 200.0); Thread.sleep(200)
        assertEquals(200.0, c.status().nozzle?.target)
        c.close()
    }

    @Test
    fun `BL6 the A1 and P1 camera logs in with the access code and gives JPEG images one after another`() {
        val c = connect()
        c.test()
        c.cameraFrames().use { f ->
            val a = f.next(); val b = f.next()
            assertTrue(a[0] == 0xFF.toByte() && a[1] == 0xD8.toByte())
            assertTrue(String(a).contains("frame 0") && String(b).contains("frame 1"))
        }
        assertEquals("bblp" to printer.accessCode, printer.cameraLogins.last())
        // A wrong code: the camera closes before any image, which is said as an access code problem.
        val bad = BambuConnection.of(printer.url, "00000000", c.seenFingerprint)
        val e = assertFailsWith<PrinterException> { bad.cameraFrames().use { it.next() } }
        assertEquals(PrinterException.Kind.AUTH, e.kind)
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
