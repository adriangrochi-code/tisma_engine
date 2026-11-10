package net.tisma.printers

import org.junit.Test
import kotlin.test.assertEquals

class DiscoveryTest {
    @Test
    fun `BD2 OctoPrint's address is the service plus its path`() {
        val p = FoundPrinter(PrinterKind.OCTOPRINT, "octopi", "192.168.1.30", 80, "/octo/")
        assertEquals("http://192.168.1.30:80/octo", p.url)
        assertEquals("http://[fe80::1]:7125", FoundPrinter(PrinterKind.MOONRAKER, "v", "fe80::1", 7125).url)
    }

    @Test
    fun `a printer seen twice, or by two services, is listed once`() {
        val list = FoundPrinters()
        list.seen(FoundPrinter(PrinterKind.OCTOPRINT, "octopi", "10.0.0.5", 80), 0)
        list.seen(FoundPrinter(PrinterKind.OCTOPRINT, "octopi", "10.0.0.5", 80), 1)
        list.seen(FoundPrinter(PrinterKind.MOONRAKER, "octopi web", "10.0.0.5", 80), 2)
        assertEquals(listOf(PrinterKind.OCTOPRINT), list.current(3).map { it.kind })
    }

    @Test
    fun `BD4 a printer not heard for 3 minutes leaves the list, and saved ones are not offered`() {
        val list = FoundPrinters()
        list.seen(FoundPrinter(PrinterKind.MOONRAKER, "voron", "10.0.0.7", 7125), 0)
        list.seen(FoundPrinter(PrinterKind.MOONRAKER, "ender", "10.0.0.8", 7125), 100_000)
        assertEquals(2, list.current(170_000).size)
        assertEquals(listOf("ender"), list.current(190_000).map { it.name })
        assertEquals(emptyList(), list.current(190_000, listOf("http://10.0.0.8:7125/")))
        list.lost("10.0.0.8", 7125)
        assertEquals(emptyList(), list.current(190_000))
    }
}
