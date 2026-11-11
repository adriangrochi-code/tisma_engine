package net.tisma.app

import kotlinx.serialization.json.JsonPrimitive
import net.tisma.app.ui.specNumber
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

/** A part's number goes as typed: 1,5 stays 1.5 even where the default was a whole 1. */
class SpecNumberTest {
    @Test
    fun `decimals are kept, whole numbers stay whole`() {
        assertEquals(JsonPrimitive(1.5), specNumber("1,5"))
        assertEquals(JsonPrimitive(1.5), specNumber("1.5"))
        assertEquals(JsonPrimitive(20L), specNumber("20"))
        assertEquals(JsonPrimitive(20.5), specNumber(" 20,5 "))
        assertNull(specNumber("abc"))
    }
}
