import net.tisma.app.engine.Calibrations
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

/** Calibration tests on the phone, the same arguments and readings as Android's. */
class CalibrationsTest {
    @Test
    fun theArgumentsWithAndWithoutARange() {
        assertEquals(listOf("calibration", "temperature", "--config", "c.json", "--model", "m.stl", "--settings", "s.json"),
            Calibrations.args("temperature", "c.json", "m.stl", "s.json"))
        assertEquals(listOf("--start", "230", "--end", "200", "--step", "5"),
            Calibrations.args("temperature", "c", "m", "s", listOf(230.0, 200.0, 5.0)).takeLast(6))
        assertEquals(listOf("calibration", "apply", "flow", "--value", "0.95", "--config", "c", "-o", "o"),
            Calibrations.applyArgs("flow", 0.95, "c", "o"))
    }

    @Test
    fun theSegmentsInBothShapes() {
        val objects = Calibrations.parse("""{"unit":"C","segments":[{"z0":0,"z1":10.4,"value":230},{"z0":10.4,"z1":20.4,"value":225}]}""")!!
        assertEquals("C", objects.unit)
        assertEquals(Calibrations.Segment(10.4, 20.4, 225.0), objects.segments[1])
        assertEquals(2, Calibrations.parse("""{"unit":"C","segments":[[0.0,10.4,230.0],[10.4,20.4,225.0]]}""")!!.segments.size)
        assertNull(Calibrations.parse("not json"))
    }

    @Test
    fun onlyTheChangedSettingsAreKept() {
        val c = Calibrations.changed("""{"temperature":{"nozzle":215},"layers":{"height":0.2}}""",
            """{"temperature":{"nozzle":225},"layers":{"height":0.2}}""")
        assertEquals(setOf("temperature.nozzle"), c.keys)
        assertEquals("225", c.getValue("temperature.nozzle").toString())
    }
}
