package net.tisma.printers

import org.junit.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

class PrintEventsTest {
    private fun s(state: PrinterState, pct: Double? = null, msg: String = "") = PrinterStatus(state, "cube.gcode", pct, message = msg)

    @Test
    fun `finished, stopped, paused and failed`() {
        assertEquals(PrintEvent.Finished("cube.gcode"), PrintEvents.between(s(PrinterState.PRINTING, 99.5), s(PrinterState.READY)))
        assertEquals(PrintEvent.Stopped("cube.gcode"), PrintEvents.between(s(PrinterState.PAUSED, 40.0), s(PrinterState.READY)))
        assertEquals(PrintEvent.Paused("cube.gcode", "Filament runout"),
            PrintEvents.between(s(PrinterState.PRINTING, 40.0), s(PrinterState.PAUSED, 40.0, "Filament runout")))
        assertEquals(PrintEvent.Failed("cube.gcode", "MCU shutdown"),
            PrintEvents.between(s(PrinterState.PRINTING, 40.0), s(PrinterState.ERROR, msg = "MCU shutdown")))
    }

    @Test
    fun `no event without a change, or when the printer did not answer`() {
        assertNull(PrintEvents.between(s(PrinterState.PRINTING, 10.0), s(PrinterState.PRINTING, 20.0)))
        assertNull(PrintEvents.between(s(PrinterState.PRINTING, 99.9), s(PrinterState.OFFLINE)))
        assertNull(PrintEvents.between(null, s(PrinterState.READY)))
        assertNull(PrintEvents.between(s(PrinterState.READY), s(PrinterState.ERROR)))
        assertNull(PrintEvents.between(s(PrinterState.PAUSED, 50.0), s(PrinterState.PRINTING, 50.0)))
    }
}
