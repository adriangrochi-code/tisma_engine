package net.tisma.printers

import net.tisma.mockprinters.MockCreality
import net.tisma.mockprinters.MockMoonraker
import org.junit.After
import org.junit.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

/** Creality with its stock firmware against the simulator (printer_connections.md, «Creality de fábrica», CR1). */
class CrealityTest {
    private val printer = MockCreality().start()
    private val opened = mutableListOf<PrinterConnection>()

    @After
    fun stop() { opened.forEach { it.close() }; printer.stop() }

    private fun connect(url: String = printer.url, windowMs: Long = 15_000) =
        CrealityConnection.of(url, net.tisma.cloud.UrlConnectionHttp, windowMs).also { opened += it }

    private fun waitFor(ms: Long = 5_000, ok: () -> Boolean) {
        val end = System.currentTimeMillis() + ms
        while (!ok() && System.currentTimeMillis() < end) Thread.sleep(20)
    }

    @Test
    fun `CR1-1 Moonraker on 7125 is used instead of the stock socket`() {
        val moonraker = MockMoonraker().start()
        try {
            val port = moonraker.url.substringAfterLast(':').toInt()
            moonraker.apiKey = null
            val c = connect("creality://127.0.0.1?ws=${printer.port}&moonraker=$port")
            assertTrue(c.test().contains("Moonraker", ignoreCase = true) || c.test().isNotBlank())
            c.status()
            assertTrue(moonraker.log.isNotEmpty())
            assertEquals(0, printer.clients.get(), "the 9999 socket is not opened")
            assertTrue(c.hasControls)
        } finally { moonraker.stop() }
    }

    @Test
    fun `CR1-2 the pushed times show as elapsed and left, with the progress from both`() {
        val c = connect()
        c.test()
        printer.push("""{"printJobTime":600,"printLeftTime":1800}""")
        waitFor { c.status().state == PrinterState.PRINTING }
        val s = c.status()
        assertEquals(600L, s.elapsedS)
        assertEquals(1800L, s.remainingS)
        assertEquals(25.0, s.percent)
    }

    @Test
    fun `CR1-3 an empty model shows the board code`() {
        assertEquals("Creality (board F012) · K1C-1234", connect().test())
    }

    @Test
    fun `CR1-4 a print that stops counting is called stopped only after the window`() {
        val c = connect(windowMs = 1_500)
        c.test()
        printer.push("""{"printJobTime":600,"printLeftTime":1800}""")
        waitFor { c.status().state == PrinterState.PRINTING }
        // The same shape as preheating: no time left reported.
        printer.push("""{"printJobTime":600,"printLeftTime":0}""")
        Thread.sleep(300)
        assertEquals(PrinterState.PRINTING, c.status().state, "not before the window")
        Thread.sleep(1_500)
        val s = c.status()
        assertEquals(PrinterState.READY, s.state)
        assertEquals("stopped", s.message)
    }

    @Test
    fun `CR1-5 a cut link mid-print is opened again and the print stays active meanwhile`() {
        val c = connect()
        c.test()
        printer.push("""{"printJobTime":60,"printLeftTime":600}""")
        waitFor { c.status().state == PrinterState.PRINTING }
        printer.stop()
        printer.cut()
        Thread.sleep(300)
        val s = c.status()
        assertEquals(PrinterState.PRINTING, s.state)
        assertEquals(PrinterException.Reason.CR_RECONNECTING, s.reason)
    }

    @Test
    fun `CR1-6 sending is not tried and the person is told to turn on Moonraker`() {
        val c = connect()
        c.test()
        val e = assertFailsWith<PrinterException> { c.upload("a.gcode", 1, true) { net.tisma.io.BytesInput(byteArrayOf(1)) } }
        assertEquals(PrinterException.Reason.CR_NO_SEND, e.reason)
        assertEquals(0, printer.otherRequests.get(), "no upload request was made")
    }

    @Test
    fun `a closed 9999 says the printer does not accept state connections`() {
        val closed = java.net.ServerSocket(0).use { it.localPort }
        val e = assertFailsWith<PrinterException> { connect("creality://127.0.0.1?ws=$closed&moonraker=$closed").status() }
        assertEquals(PrinterException.Reason.CR_REFUSED, e.reason)
    }

    @Test
    fun `Anycubic is not offered and says to use its own program`() {
        val e = assertFailsWith<PrinterException> { AnycubicConnection().test() }
        assertEquals(PrinterException.Reason.AC_USE_SLICER_NEXT, e.reason)
    }
}
