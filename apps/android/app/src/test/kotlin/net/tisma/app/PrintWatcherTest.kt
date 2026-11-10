package net.tisma.app

import net.tisma.app.printers.PrintWatcher
import net.tisma.printers.PrintEvent
import net.tisma.printers.PrinterState
import net.tisma.printers.PrinterStatus
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertIs
import kotlin.test.assertTrue

/** The print watch stops by itself, also when a printer goes silent mid print. */
class PrintWatcherTest {
    private fun s(state: PrinterState) = PrinterStatus(state, "cube.gcode", 50.0)

    @Test
    fun `a printer switched off mid print stops counting after its rounds`() {
        val w = PrintWatcher(maxMissed = 3)
        assertTrue(w.observe("a", s(PrinterState.PRINTING)).second)
        assertTrue(w.observe("a", null).second)
        assertTrue(w.observe("a", s(PrinterState.OFFLINE)).second)
        // Third silent round: no longer counted as printing, so the watch can end.
        assertFalse(w.observe("a", null).second)
    }

    @Test
    fun `an answer resets the count, and the end of a print is told`() {
        val w = PrintWatcher(maxMissed = 2)
        w.observe("a", s(PrinterState.PRINTING))
        w.observe("a", null)
        assertTrue(w.observe("a", PrinterStatus(PrinterState.PRINTING, "cube.gcode", 99.5)).second)
        val (event, active) = w.observe("a", PrinterStatus(PrinterState.READY, "cube.gcode", 100.0))
        assertIs<PrintEvent.Finished>(event)
        assertFalse(active)
        assertEquals(false, w.observe("b", null).second)
    }
}
