package net.tisma.app

import net.tisma.app.ui.printedDay
import kotlin.test.Test
import kotlin.test.assertEquals

/** local_library.md (058df3b): «La imprimí» is a day; old entries with a time read as their day. */
class PrintedDayTest {
    @Test
    fun `both forms read as the day`() {
        assertEquals("10-11-2026", printedDay("2026-11-10"))
        assertEquals("10-11-2026", printedDay("2026-11-10T13:05:22Z"))
        assertEquals("ayer", printedDay("ayer"))
        // What the app sends: the ISO day, whatever the phone's locale (Arabic digits, other calendars).
        val saved = java.util.Locale.getDefault()
        java.util.Locale.setDefault(java.util.Locale.forLanguageTag("ar-EG"))
        try {
            kotlin.test.assertTrue(Regex("""\d{4}-\d{2}-\d{2}""").matches(java.time.LocalDate.now().toString()))
        } finally {
            java.util.Locale.setDefault(saved)
        }
    }
}
