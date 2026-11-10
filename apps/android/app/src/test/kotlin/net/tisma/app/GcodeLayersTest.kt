package net.tisma.app

import net.tisma.app.engine.BinaryGcode
import net.tisma.app.engine.GcodeLayers
import java.io.File
import org.junit.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
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

    @Test
    fun `radius arcs, helical arcs and tabs are drawn`() {
        // The same half circle as above, in Marlin's radius form, with tabs between the words.
        val r = read("G1 Z0.2\nG1 X10 Y0\n;TYPE:Perimeter\nG3\tX-10\tY0\tR10\tE1\n")
        val s = r.layers[0].segments
        assertTrue(r.layers[0].count >= 18)
        assertEquals(-10f, s[s.size - 2], 1e-4f)
        assertTrue((0 until r.layers[0].count).all { s[4 * it + 3] >= -1e-3f }, "counter-clockwise goes through y > 0")
        // Clockwise with R goes the other way, through y < 0.
        val cw = read("G1 Z0.2\nG1 X10 Y0\nG2 X-10 Y0 R10 E1\n").layers[0].segments
        assertTrue((0 until cw.size / 4).all { cw[4 * it + 3] <= 1e-3f })
        // A helical arc climbing to Z 0.4 belongs to the 0.4 layer, and the next move continues from there.
        val h = read("G1 Z0.2\nG1 X10 Y0\nG1 X10 Y5 E1\nG3 X-10 Y5 I-10 J0 Z0.4 E2\nG1 X-10 Y8 E3\n")
        assertEquals(listOf(0.2f, 0.4f), h.layers.map { it.z })
    }

    @Test
    fun `AND3 the engine's own G-code reads (cube, 50 layers)`() {
        val text = javaClass.getResourceAsStream("/engine_cube.gcode")!!.bufferedReader()
        val g = GcodeLayers.read(text)
        assertEquals(50, g.layers.size)
        assertTrue(g.layers.all { it.count > 0 })
    }

    @Test
    fun `binary G-code from the engine reads (cube, 50 layers)`() {
        val f = File.createTempFile("cube", ".bgcode")
        javaClass.getResourceAsStream("/engine_cube.bgcode")!!.use { i -> f.outputStream().use { i.copyTo(it) } }
        val g = BinaryGcode.open(f).bufferedReader().use { GcodeLayers.read(it) }
        assertEquals(50, g.layers.size)
        assertTrue(g.layers.all { it.count > 0 })
        // One flipped byte in the last block's data fails its CRC32.
        val bytes = f.readBytes(); bytes[bytes.size - 10] = (bytes[bytes.size - 10].toInt() xor 1).toByte(); f.writeBytes(bytes)
        assertFailsWith<BinaryGcode.Unreadable> { BinaryGcode.open(f).bufferedReader().use { GcodeLayers.read(it) } }
        f.delete()
    }

    @Test
    fun `width and speed come from WIDTH tags and the feed rate`() {
        val g = read("""
            G90
            M83
            ;LAYER_CHANGE
            ;Z:0.2
            ;TYPE:Perimeter
            ;WIDTH:0.45
            G1 X0 Y0 F6000
            G1 X10 Y0 E1 F1800
            ;WIDTH:0.6
            G1 X10 Y10 E1
            G2 X0 Y10 I-5 J0 E1 F600
        """)
        val l = g.layers.single()
        assertEquals(0.45f, l.widths[0], 1e-6f)
        assertEquals(30f, l.speeds[0], 1e-4f)
        assertEquals(0.6f, l.widths[1], 1e-6f)
        assertEquals(30f, l.speeds[1], 1e-4f)
        assertEquals(10f, l.speeds[l.count - 1], 1e-4f)
        assertEquals(0.45f..0.6f, g.widthRange)
        assertEquals(10f..30f, g.speedRange)
    }

    @Test
    fun `3D lines stand at each layer's Z, colored, up to the chosen layer`() {
        val text = javaClass.getResourceAsStream("/engine_cube.gcode")!!.bufferedReader()
        val g = GcodeLayers.read(text)
        val upTo = 9
        val all = g.lines3d(upTo) { _, _ -> 0xFF8000 }
        assertEquals(12 * (0..upTo).sumOf { g.layers[it].count }, all.size)
        assertEquals(g.layers[0].z, all[2])
        assertEquals(g.layers[upTo].z, all[all.size - 4])
        assertEquals(1f, all[3]); assertEquals(128 / 255f, all[4], 1e-6f); assertEquals(0f, all[5])
        // Capped: only the top layers that fit, and always the top one.
        val capped = g.lines3d(upTo, maxSegments = g.layers[upTo].count + g.layers[upTo - 1].count) { _, _ -> 0 }
        assertEquals(12 * (g.layers[upTo].count + g.layers[upTo - 1].count), capped.size)
        assertEquals(12 * g.layers[upTo].count, g.lines3d(upTo, maxSegments = 1) { _, _ -> 0 }.size)
    }
}
