package net.tisma.app

import net.tisma.app.ui.calibrationRange
import net.tisma.app.ui.shown
import java.util.Locale
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertNull

/** A calibration's range is used only when it is whole and can run; its values read in the person's language. */
class CalibrationRangeTest {
    @Test
    fun `whole ranges with comma or point, and the ones that cannot run`() {
        assertContentEquals(doubleArrayOf(0.9, 1.1, 0.05), calibrationRange(listOf("0,9", "1.1", "0,05")))
        assertNull(calibrationRange(listOf("0,9", "", "")))
        assertNull(calibrationRange(listOf("200", "240", "0")))
        assertNull(calibrationRange(listOf("200", "200", "5")))
        assertNull(calibrationRange(listOf("200", "abc", "5")))
    }

    @Test
    fun `values with a decimal comma in Spanish`() {
        val saved = Locale.getDefault()
        try {
            Locale.setDefault(Locale.forLanguageTag("es-AR")); assertEquals("0,95", shown(0.95))
            Locale.setDefault(Locale.ENGLISH); assertEquals("0.95", shown(0.95))
            assertEquals("215", shown(215.0))
        } finally { Locale.setDefault(saved) }
    }
}
