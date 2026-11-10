package net.tisma.app

import net.tisma.app.printers.PrintersState
import net.tisma.app.ui.Connection
import net.tisma.app.ui.connectionOf
import net.tisma.printers.FoundPrinter
import net.tisma.printers.PrinterConfig
import net.tisma.printers.PrinterKind
import net.tisma.printers.PrinterState
import net.tisma.printers.PrinterStatus
import kotlin.test.Test
import kotlin.test.assertEquals

/** settings_panel.md, rule 42: the printer card's connection icon follows the printer to send to. */
class ConnectionIconTest {
    private fun printer(id: String) = PrinterConfig(id, "P$id", PrinterKind.MOONRAKER, "http://10.0.0.$id")
    private fun status(s: PrinterState) = PrinterStatus(s)

    @Test
    fun `none, found, connecting, connected and error`() {
        assertEquals(Connection.NONE, connectionOf(PrintersState(), null))
        assertEquals(Connection.FOUND, connectionOf(PrintersState(found = listOf(FoundPrinter(PrinterKind.MOONRAKER, "k", "10.0.0.9", 80))), null))
        val two = listOf(printer("1"), printer("2"))
        assertEquals(Connection.CONNECTING, connectionOf(PrintersState(printers = two), "2"))
        assertEquals(Connection.CONNECTED, connectionOf(PrintersState(printers = two, status = mapOf("2" to status(PrinterState.PRINTING))), "2"))
        assertEquals(Connection.ERROR, connectionOf(PrintersState(printers = two, status = mapOf("2" to status(PrinterState.OFFLINE),
            "1" to status(PrinterState.READY))), "2"))
        // No printer chosen: the first saved one.
        assertEquals(Connection.CONNECTED, connectionOf(PrintersState(printers = two, status = mapOf("1" to status(PrinterState.READY))), null))
    }
}
