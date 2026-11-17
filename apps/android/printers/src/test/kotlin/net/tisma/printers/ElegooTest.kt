package net.tisma.printers

import net.tisma.mockprinters.MockElegoo
import org.junit.After
import org.junit.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

/** Elegoo over SDCP v3 against the simulator (printer_connections.md, «Impresoras Elegoo por red local», EL1). */
class ElegooTest {
    private val printer = MockElegoo().start()
    private val opened = mutableListOf<ElegooConnection>()

    @After
    fun stop() { opened.forEach { it.close() }; printer.stop() }

    private fun connect(pongMs: Long = 20_000) =
        ElegooConnection("127.0.0.1", printer.port, net.tisma.cloud.UrlConnectionHttp, answerMs = 3_000, pongMs = pongMs).also { opened += it }

    private fun waitFor(ms: Long = 5_000, ok: () -> Boolean) {
        val end = System.currentTimeMillis() + ms
        while (!ok() && System.currentTimeMillis() < end) Thread.sleep(20)
    }

    @Test
    fun `SHA-1 matches the standard's digest`() {
        for (text in listOf("", "abc", "a".repeat(1000), "dGhlIHNhbXBsZSBub25jZQ==258EAFA5-E914-47DA-95CA-C5AB0DC85B11")) {
            assertTrue(java.security.MessageDigest.getInstance("SHA-1").digest(text.toByteArray())
                .contentEquals(net.tisma.io.Sha1.digest(text.toByteArray())), text)
        }
    }

    @Test
    fun `EL1_1 two equal answers to M99999 are one printer with its name, IP and board id`() {
        val found = ElegooProbe.probe(listOf(java.net.InetSocketAddress("127.0.0.1", printer.answerDiscovery(times = 2))), 1_500)
        val p = found.single()
        assertEquals(PrinterKind.ELEGOO, p.kind)
        assertEquals("Centauri Carbon", p.name)
        assertEquals("127.0.0.1", p.host)
        assertEquals(printer.mainboardId, p.serial)
        assertEquals("elegoo://127.0.0.1", p.url)
    }

    @Test
    fun `the state and the board id come from the printer's own pushes`() {
        val c = connect()
        assertEquals("Elegoo Centauri Carbon V1.1.29", c.test())
        assertEquals(printer.mainboardId, c.mainboardId)
        val s = c.status()
        assertEquals(PrinterState.READY, s.state)
        assertEquals(Temperature(25.0, 0.0), s.nozzle)
    }

    @Test
    fun `EL1_2 a 2_5 MB file goes in three pieces with one Uuid, the total size and the whole MD5`() {
        val bytes = ByteArray(2_621_440) { (it % 251).toByte() }
        connect().upload("part.gcode", bytes.size.toLong(), start = true) { net.tisma.io.BytesInput(bytes) }
        assertEquals(listOf(0L, 1_048_576L, 2_097_152L), printer.pieces.map { it.offset })
        assertEquals(1, printer.pieces.map { it.uuid }.toSet().size)
        assertTrue(printer.pieces.all { it.totalSize == bytes.size.toLong() })
        val md5 = java.security.MessageDigest.getInstance("MD5").digest(bytes).joinToString("") { "%02x".format(it) }
        assertTrue(printer.pieces.all { it.md5 == md5 && it.check == "1" })
        assertTrue(printer.file().contentEquals(bytes))
        // Then 128 starts it.
        assertEquals(128, printer.requests.last().first)
        waitFor { printer.current == 1 }
        assertEquals(PrinterState.PRINTING, opened.single().status().state)
    }

    @Test
    fun `EL1_3 a refused second piece stops the upload and says the offset does not match`() {
        printer.failPiece = 1
        val bytes = ByteArray(2_621_440)
        val e = assertFailsWith<PrinterException> { connect().upload("part.gcode", bytes.size.toLong(), start = true) { net.tisma.io.BytesInput(bytes) } }
        assertEquals(PrinterException.Reason.EL_UPLOAD_FAILED + "-2", e.reason)
        assertEquals(2, printer.pieces.size, "nothing after the refused piece")
        assertTrue(printer.requests.none { it.first == 128 })
    }

    @Test
    fun `EL1_4 a busy printer is said and not shown as printing`() {
        val bytes = ByteArray(10)
        val c = connect()
        printer.busy = true
        val e = assertFailsWith<PrinterException> { c.upload("a.gcode", 10, start = true) { net.tisma.io.BytesInput(bytes) } }
        assertEquals(PrinterException.Kind.CONFLICT, e.kind)
        assertEquals(PrinterException.Reason.EL_ACK_PRINT + "1", e.reason)
        assertEquals(PrinterState.READY, c.status().state)
    }

    @Test
    fun `EL1_5 pause and stop show paused and stopped, and each answer has its RequestID`() {
        val bytes = ByteArray(10)
        val c = connect()
        c.upload("a.gcode", 10, start = true) { net.tisma.io.BytesInput(bytes) }
        c.pause()
        waitFor { printer.pushed.contains(6) }
        assertEquals(PrinterState.PAUSED, c.status().state)
        c.cancel()
        waitFor { printer.pushed.contains(8) }
        val s = c.status()
        assertEquals(PrinterState.READY, s.state)
        assertEquals("stopped", s.message)
        assertEquals(false, s.finished)
        assertEquals(listOf(5, 6, 7, 8), printer.pushed.filter { it in 5..8 })
        assertEquals(printer.requests.map { it.second }, printer.answered.toList())
        assertTrue(printer.requests.map { it.second }.toSet().size == printer.requests.size, "a RequestID per request")
    }

    @Test
    fun `EL1_6 no pong opens the link again and keeps the last state meanwhile`() {
        val c = connect(pongMs = 1_500)
        assertEquals(PrinterState.READY, c.status().state)
        printer.silent = true
        printer.current = 1; printer.printStatus = 3
        // The link is dropped for want of pongs; the next read opens a new one.
        waitFor(15_000) { printer.clients.get() == 0 }
        printer.silent = false
        val s = c.status()
        assertEquals(PrinterState.PRINTING, s.state)
        assertTrue(printer.pings.get() >= 1)
    }

    @Test
    fun `EL1_6 a sixth connection is refused with too many connections`() {
        printer.maxClients = 0
        val e = assertFailsWith<PrinterException> { connect().test() }
        assertEquals(PrinterException.Reason.EL_TOO_MANY, e.reason)
    }

    @Test
    fun `a cut link during a print is opened again and the print still shows`() {
        val c = connect()
        printer.current = 1; printer.printStatus = 3
        c.status()
        printer.cut()
        waitFor { printer.clients.get() == 0 }
        assertEquals(PrinterState.PRINTING, c.status().state)
    }

    @Test
    fun `a resin printer is never sent G-code, only the engine's goo`() {
        val resin = MockElegoo(name = "Saturn", machine = "Saturn 3 Ultra").start()
        try {
            val c = ElegooConnection("127.0.0.1", resin.port, net.tisma.cloud.UrlConnectionHttp, answerMs = 3_000).also { opened += it }
            assertEquals(false, c.status().acceptsGcode)
            val e = assertFailsWith<PrinterException> { c.upload("a.gcode", 3, false) { net.tisma.io.BytesInput(byteArrayOf(1, 2, 3)) } }
            assertEquals(PrinterException.Reason.EL_RESIN_NO_GCODE, e.reason)
            assertEquals(0, resin.pieces.size, "nothing was sent")
            c.upload("a.goo", 3, false) { net.tisma.io.BytesInput(byteArrayOf(1, 2, 3)) }
            assertEquals(1, resin.pieces.size)
        } finally { resin.stop() }
        assertEquals(true, connect().status().acceptsGcode, "the Centauri Carbon takes G-code")
    }

    @Test
    fun `a pushed print error is the printer's error`() {
        val s = ElegooConnection.statusOf(kotlinx.serialization.json.Json.parseToJsonElement(
            """{"CurrentStatus":[1],"PrintInfo":{"Status":3,"ErrorNumber":4,"CurrentLayer":5,"TotalLayer":10}}""") as kotlinx.serialization.json.JsonObject)
        assertEquals(PrinterState.ERROR, s.state)
        assertEquals(PrinterException.Reason.EL_PRINT_ERROR + "4", s.reason)
        val p = ElegooConnection.statusOf(kotlinx.serialization.json.Json.parseToJsonElement(
            """{"Status":{"CurrentStatus":[1],"Data":{"PrintInfo":{"Status":3,"CurrentLayer":25,"TotalLayer":100}}}}""") as kotlinx.serialization.json.JsonObject)
        assertEquals(25.0, assertNotNull(p.percent), "fields nested deeper are found")
    }
}
