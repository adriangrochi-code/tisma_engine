package net.tisma.app

import net.tisma.app.engine.GcodeLayers
import org.junit.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class GcodeLayersTest {
    private fun read(text: String) = GcodeLayers.read(text.trimIndent().reader().buffered())

    @Test
    fun `layers and types come from the engine's tags, and travel is left out`() {
        val g = read("""
            G90
            M83
            ;LAYER_CHANGE
            ;Z:0.2
            ;HEIGHT:0.2
            G1 Z0.2 F600
            G0 X10 Y10
            ;TYPE:External perimeter
            G1 X20 Y10 E0.5
            G1 X20 Y20 E0.5
            ;TYPE:Internal infill
            G1 X10 Y20 E0.5
            G0 X0 Y0
            ;LAYER_CHANGE
            ;Z:0.4
            ;HEIGHT:0.2
            G1 Z0.4
            ;TYPE:External perimeter
            G1 X5 Y0 E0.2
        """)
        assertEquals(2, g.layers.size)
        assertEquals(0.2f, g.layers[0].z)
        assertEquals(0.2f, g.layers[1].height)
        assertEquals(3, g.layers[0].count)
        assertEquals(listOf("External perimeter", "Internal infill"), g.types)
        assertEquals(listOf<Byte>(0, 0, 1), g.layers[0].segmentTypes.toList())
        assertEquals(listOf(10f, 10f, 20f, 10f), g.layers[0].segments.take(4))
        assertEquals(listOf(0f, 0f, 20f, 20f), g.bounds.toList())
    }

    @Test
    fun `absolute extrusion with G92 resets, and retractions do not draw`() {
        val g = read("""
            M82
            G92 E0
            G1 Z0.3
            ;TYPE:Perimeter
            G1 X10 Y0 E1
            G1 E0.2
            G0 X10 Y10
            G1 E1
            G1 X0 Y10 E2
            G92 E0
            G1 X0 Y0 E0.5
        """)
        assertEquals(1, g.layers.size)
        assertEquals(3, g.layers[0].count)
        assertEquals(0.3f, g.layers[0].z)
    }

    @Test
    fun `an arc becomes short chords that end where it ends`() {
        val g = read("""
            G1 Z0.2
            G1 X10 Y0
            ;TYPE:Perimeter
            G3 X-10 Y0 I-10 J0 E1
        """)
        val s = g.layers[0].segments
        assertTrue(g.layers[0].count >= 18)
        assertEquals(-10f, s[s.size - 2], 1e-4f)
        assertEquals(0f, s[s.size - 1], 1e-4f)
        // Counter-clockwise from (10,0) to (-10,0) around (0,0) goes through y > 0.
        assertTrue((0 until g.layers[0].count).all { s[4 * it + 3] >= -1e-4f })
    }
}
