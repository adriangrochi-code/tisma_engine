import net.tisma.app.engine.GcodeLayers
import net.tisma.io.BytesInput
import net.tisma.io.lines
import kotlin.test.Test
import kotlin.test.assertEquals

/** The preview's parser and line reader, the same on Android and iOS. */
class GcodeCommonTest {
    @Test
    fun linesSplitOnNewlinesOfBothKinds() {
        assertEquals(listOf("G90", "M83", "", "G1 X1"), BytesInput("G90\r\nM83\n\nG1 X1".encodeToByteArray()).lines().toList())
        val long = "A".repeat(200_000)
        assertEquals(listOf(long, "B"), BytesInput("$long\nB\n".encodeToByteArray()).lines().toList())
    }

    @Test
    fun layersFromLines() {
        val g = GcodeLayers.read(BytesInput("""
            M83
            ;LAYER_CHANGE
            ;Z:0.2
            G1 Z0.2
            ;TYPE:External perimeter
            G1 X10 Y0 E0.5
            G1 X10 Y10 E0.5
            ;LAYER_CHANGE
            ;Z:0.4
            G1 Z0.4
            G1 X0 Y10 E0.5
        """.trimIndent().encodeToByteArray()).lines())
        assertEquals(2, g.layers.size)
        assertEquals(2, g.layers[0].count)
        assertEquals(0.4f, g.layers[1].z)
    }
}
