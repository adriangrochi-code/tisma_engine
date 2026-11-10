package net.tisma.app

import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonPrimitive
import net.tisma.app.ui.BedFamily
import net.tisma.app.ui.PrinterShape
import net.tisma.app.ui.bedFamily
import net.tisma.app.ui.highFlow
import net.tisma.app.ui.printerShape
import kotlin.math.cos
import kotlin.math.sin
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** settings_panel.md, rules 34 to 36: which drawing each printer, bed and nozzle gets. */
class DrawingsTest {
    private fun shape(vararg kv: Pair<String, JsonElement>) = printerShape { k -> kv.toMap()[k] }
    private fun points(vararg p: Pair<Double, Double>) = JsonArray(p.map { JsonArray(listOf(JsonPrimitive(it.first), JsonPrimitive(it.second))) })

    @Test
    fun `the printer's drawing follows rule 34's table in its order`() {
        assertEquals(PrinterShape.OPEN, shape())
        assertEquals(PrinterShape.RESIN, shape("machine.technology" to JsonPrimitive("msla"), "machine.enclosed" to JsonPrimitive(true)))
        assertEquals(PrinterShape.PELLETS, shape("machine.feedstock" to JsonPrimitive("pellet")))
        assertEquals(PrinterShape.BELT, shape("belt.enabled" to JsonPrimitive(true)))
        assertEquals(PrinterShape.ENCLOSED, shape("machine.enclosed" to JsonPrimitive(true)))
        val circle = points(*(0 until 32).map { 100 * cos(it * Math.PI / 16) to 100 * sin(it * Math.PI / 16) }.toTypedArray())
        assertEquals(PrinterShape.DELTA, shape("machine.bed" to circle, "machine.enclosed" to JsonPrimitive(true)))
        assertEquals(PrinterShape.OPEN, shape("machine.bed" to points(0.0 to 0.0, 220.0 to 0.0, 220.0 to 220.0, 0.0 to 220.0)))
    }

    @Test
    fun `the bed's family by its name, and high-flow nozzles`() {
        assertEquals(BedFamily.TEXTURED, bedFamily("Textured PEI Plate"))
        assertEquals(BedFamily.TEXTURED, bedFamily("Textured Cool Plate"))
        assertEquals(BedFamily.SMOOTH, bedFamily("Smooth PEI Plate"))
        assertEquals(BedFamily.GLASS, bedFamily("Vidrio"))
        assertEquals(BedFamily.COOL, bedFamily("Cool Plate"))
        assertEquals(BedFamily.ENGINEERING, bedFamily("High Temp Plate"))
        assertEquals(BedFamily.ENGINEERING, bedFamily("Engineering Plate"))
        assertEquals(BedFamily.OTHER, bedFamily("Supertack Plate"))
        assertTrue(highFlow("Bambu Lab H2D 0.4 HF"))
        assertTrue(highFlow("Prusa Core One 0.6 High Flow"))
        assertFalse(highFlow("Prusa MK4S HFX? no"))
    }
}
