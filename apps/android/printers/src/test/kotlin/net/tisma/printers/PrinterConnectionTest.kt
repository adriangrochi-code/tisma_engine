package net.tisma.printers

import net.tisma.cloud.UrlConnectionHttp
import net.tisma.mockprinters.MockMoonraker
import net.tisma.mockprinters.MockOctoPrint
import net.tisma.mockprinters.SimulatedPrinter
import org.junit.After
import org.junit.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The clients against simulators written from the APIs' documentation (N1, MA7, CM1). */
class PrinterConnectionTest {
    private val started = mutableListOf<SimulatedPrinter>()

    @After
    fun stop() = started.forEach { it.stop() }

    private fun <T : SimulatedPrinter> sim(p: T): T = p.also { it.start(); started += it }

    private val gcode = "G28\nG1 Z0.2 F600\nG1 X10 Y10 E1\n".toByteArray()

    /** The common walk: idle, upload and start, progress, pause, resume, cancel. */
    private fun walk(c: PrinterConnection, printer: SimulatedPrinter) {
        assertEquals(PrinterState.READY, c.status().state)
        c.upload("cube.gcode", gcode.size.toLong(), start = true) { gcode.inputStream() }
        assertTrue(printer.files.getValue("cube.gcode").contentEquals(gcode))
        val s = c.status()
        assertEquals(PrinterState.PRINTING, s.state)
        assertEquals("cube.gcode", s.file)
        assertEquals(25.0, assertNotNull(s.percent), 0.001)
        assertEquals(Temperature(210.0, 210.0), s.nozzle)
        assertEquals(Temperature(60.0, 60.0), s.bed)
        assertNotNull(s.remainingS)
        c.pause()
        assertEquals(PrinterState.PAUSED, c.status().state)
        c.resume()
        assertEquals(PrinterState.PRINTING, c.status().state)
        c.cancel()
        val after = c.status()
        assertEquals(PrinterState.READY, after.state)
        assertNull(after.percent)
        assertEquals(0.0, after.nozzle!!.target)
    }

    @Test
    fun `N1 Moonraker test, upload and print, pause, resume, cancel`() {
        val m = sim(MockMoonraker())
        val c = PrinterConfig("1", "Voron", PrinterKind.MOONRAKER, m.url).connect(null)
        assertTrue(c.test().startsWith("Moonraker v0.9.3-sim"))
        walk(c, m)
        assertTrue(m.log.any { it == ("POST" to "/server/files/upload") })
    }

    @Test
    fun `N1 OctoPrint test, upload and print, pause, resume, cancel`() {
        val o = sim(MockOctoPrint())
        val c = PrinterConfig("2", "Ender", PrinterKind.OCTOPRINT, o.url).connect("test-key")
        assertEquals("OctoPrint 1.10.2", c.test())
        walk(c, o)
        assertTrue(o.keysSeen.all { it == "test-key" })
    }

    @Test
    fun `N1 a wrong key is an authentication error`() {
        val o = sim(MockOctoPrint())
        val e = assertFailsWith<PrinterException> { OctoPrintConnection(o.url, "wrong", UrlConnectionHttp).test() }
        assertEquals(PrinterException.Kind.AUTH, e.kind)
        assertEquals("Invalid API key", e.detail)
        val m = sim(MockMoonraker()).apply { apiKey = "k" }
        assertEquals(PrinterException.Kind.AUTH,
            assertFailsWith<PrinterException> { MoonrakerConnection(m.url, null, UrlConnectionHttp).status() }.kind)
        assertEquals("Unauthorized",
            assertFailsWith<PrinterException> { MoonrakerConnection(m.url, null, UrlConnectionHttp).status() }.detail)
    }

    @Test
    fun `a print runs to the end and the printer is ready again`() {
        val m = sim(MockMoonraker())
        val c = MoonrakerConnection(m.url, null, UrlConnectionHttp)
        c.upload("a.gcode", gcode.size.toLong(), start = true) { gcode.inputStream() }
        val seen = (1..5).map { c.status() }
        assertEquals(listOf(25.0, 50.0, 75.0), seen.take(3).map { it.percent })
        assertEquals(PrinterState.READY, seen[3].state)
        assertEquals(75, seen[2].layer)
        assertEquals(100, seen[2].totalLayers)
    }

    @Test
    fun `upload without starting leaves the printer idle`() {
        val o = sim(MockOctoPrint())
        val c = OctoPrintConnection(o.url, "test-key", UrlConnectionHttp)
        c.upload("b.gcode", gcode.size.toLong(), start = false) { gcode.inputStream() }
        assertEquals(PrinterState.READY, c.status().state)
        assertTrue("b.gcode" in o.files)
    }

    @Test
    fun `a board that is not connected shows as offline or error with its reason`() {
        val o = sim(MockOctoPrint()).apply { disconnected = true }
        val s = OctoPrintConnection(o.url, "test-key", UrlConnectionHttp).status()
        assertEquals(PrinterState.OFFLINE, s.state)
        assertEquals("Printer is not operational", s.message)
        val m = sim(MockMoonraker()).apply { disconnected = true }
        val k = MoonrakerConnection(m.url, null, UrlConnectionHttp).status()
        assertEquals(PrinterState.ERROR, k.state)
        assertTrue(k.message.contains("shutdown"))
    }

    @Test
    fun `pausing when not printing is refused with the printer's words`() {
        val o = sim(MockOctoPrint())
        val e = assertFailsWith<PrinterException> { OctoPrintConnection(o.url, "test-key", UrlConnectionHttp).pause() }
        assertEquals(PrinterException.Kind.CONFLICT, e.kind)
        val m = sim(MockMoonraker())
        assertEquals("Print is not in progress",
            assertFailsWith<PrinterException> { MoonrakerConnection(m.url, null, UrlConnectionHttp).pause() }.detail)
    }

    @Test
    fun `no printer at the address is a network error`() {
        val m = sim(MockMoonraker())
        val url = m.url
        m.stop()
        assertEquals(PrinterException.Kind.NETWORK,
            assertFailsWith<PrinterException> { MoonrakerConnection(url, null, UrlConnectionHttp).status() }.kind)
    }

    @Test
    fun `CM1 cameras come with absolute addresses`() {
        val m = sim(MockMoonraker())
        val cam = assertNotNull(MoonrakerConnection(m.url, null, UrlConnectionHttp).camera())
        assertEquals("http://127.0.0.1/webcam/?action=stream", cam.streamUrl)
        assertTrue(Snapshot.fetch(cam.snapshotUrl).contentEquals(m.snapshot))
        val o = sim(MockOctoPrint())
        val oc = assertNotNull(OctoPrintConnection(o.url, "test-key", UrlConnectionHttp).camera())
        assertEquals(o.url + "/webcam/?action=stream", oc.streamUrl)
        assertEquals(o.url + "/snapshot.jpg", oc.snapshotUrl)
        o.hasCamera = false
        assertNull(OctoPrintConnection(o.url, "test-key", UrlConnectionHttp).camera())
        m.hasCamera = false
        assertNull(MoonrakerConnection(m.url, null, UrlConnectionHttp).camera())
        assertEquals(PrinterException.Kind.UNEXPECTED, assertFailsWith<PrinterException> { Snapshot.fetch("rtsp://x") }.kind)
    }

    @Test
    fun `addresses get http and the kind's port`() {
        assertEquals("http://192.168.1.20:7125", PrinterConfig.normalizeUrl("192.168.1.20", PrinterKind.MOONRAKER))
        assertEquals("http://192.168.1.20", PrinterConfig.normalizeUrl("192.168.1.20/", PrinterKind.OCTOPRINT))
        assertEquals("http://voron.local:7130", PrinterConfig.normalizeUrl("voron.local:7130", PrinterKind.MOONRAKER))
        assertEquals("https://octo.example/octo", PrinterConfig.normalizeUrl("https://octo.example/octo/", PrinterKind.OCTOPRINT))
        assertEquals("http://pi.local:7125/x", PrinterConfig.normalizeUrl("pi.local/x", PrinterKind.MOONRAKER))
    }
}
