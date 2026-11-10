package net.tisma.app

import kotlinx.serialization.json.JsonPrimitive
import net.tisma.app.ui.plateCost
import org.junit.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

/** mobile_app.md, rule 12: the result's cost from the filament's grams and its price per kg. */
class PlateCostTest {
    @Test
    fun `the cost is grams times the price per kilogram, and nothing without a price`() {
        assertEquals(0.75, plateCost(25.0, JsonPrimitive(30.0))!!, 1e-9)
        assertNull(plateCost(25.0, JsonPrimitive(0.0)))
        assertNull(plateCost(25.0, null))
        assertNull(plateCost(null, JsonPrimitive(30.0)))
    }
}
