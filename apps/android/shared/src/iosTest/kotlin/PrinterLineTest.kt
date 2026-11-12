package net.tisma.printers

import kotlin.test.Test
import kotlin.test.assertEquals

/** The iPhone's print notices come from the same rules as Android's watcher (PrintEvents). */
class PrinterLineTest {
    private val printing = PrinterStatus(PrinterState.PRINTING, "cube.gcode", 99.5)

    @Test
    fun finishedWhenThePrintEndsAtTheEnd() {
        val l = PrinterLine.of(printing, PrinterStatus(PrinterState.READY, "cube.gcode"))
        assertEquals("finished", l.event)
        assertEquals("cube.gcode", l.eventDetail)
        assertEquals("ready", l.state)
    }

    @Test
    fun stoppedBeforeTheEndCarriesThePrintersWords() {
        val l = PrinterLine.of(printing.copy(percent = 40.0), PrinterStatus(PrinterState.BUSY, "cube.gcode", message = "SLICING"))
        assertEquals("stopped", l.event)
        assertEquals("cube.gcode · SLICING", l.eventDetail)
    }

    @Test
    fun pausedAndFailedSayWhy() {
        assertEquals("Filament runout", PrinterLine.of(printing, PrinterStatus(PrinterState.PAUSED, "cube.gcode", message = "Filament runout")).eventDetail)
        assertEquals("failed", PrinterLine.of(printing, PrinterStatus(PrinterState.ERROR, "cube.gcode", message = "Thermal runaway")).event)
    }

    @Test
    fun noEventOnTheFirstReadingOrWhileNothingChanges() {
        assertEquals("", PrinterLine.of(null, printing).event)
        assertEquals("", PrinterLine.of(printing, printing).event)
        assertEquals(99.5, PrinterLine.of(null, printing).percent)
        assertEquals(-1.0, PrinterLine.of(null, PrinterStatus(PrinterState.READY)).percent)
    }
}
