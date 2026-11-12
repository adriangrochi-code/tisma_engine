import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import net.tisma.app.engine.EngineeringCase
import kotlin.test.Test
import kotlin.test.assertEquals

/** Engineering's case and report, the same on Android and the iPhone (docs/ANALYSIS.md). */
class EngineeringCaseTest {
    @Test
    fun namedFacesGoAsTheEnginesFacesWithTheForceIntoThem() {
        val down = EngineeringCase.FACES.getValue("top")
        val case = EngineeringCase(listOf(
            EngineeringCase.Condition(EngineeringCase.Kind.FIXED, IntArray(0), FloatArray(6), face = "bottom"),
            EngineeringCase.Condition(EngineeringCase.Kind.LOAD, IntArray(0), FloatArray(6), newtons = 50.0, direction = down, face = "top"),
        ), quality = "normal").toJson(FloatArray(3))
        val fixed = (case["fixtures"] as JsonArray).single() as JsonObject
        assertEquals("bottom", (fixed["face"] as JsonPrimitive).content)
        val load = (case["loads"] as JsonArray).single() as JsonObject
        assertEquals("top", (load["face"] as JsonPrimitive).content)
        assertEquals(listOf(0.0, 0.0, -50.0), (load["force"] as JsonArray).map { (it as JsonPrimitive).content.toDouble() })
    }

    @Test
    fun theVerdictComesFromTheMinimumSafety() {
        val holds = EngineeringCase.reportLines("""{"valid":true,"result":{"min_safety":3.1,"max_displacement_mm":0.02}}""", 2.0)
        assertEquals("verdict" to "holds", holds.first())
        assertEquals("3.1", holds.toMap()["min_safety"])
        assertEquals("fails", EngineeringCase.reportLines("""{"valid":true,"result":{"min_safety":1.5}}""", 2.0).toMap()["verdict"])
    }

    @Test
    fun withoutAResultTheWarningsSayWhy() {
        val l = EngineeringCase.reportLines("""{"valid":false,"warnings":[{"message":"nothing holds the part"}]}""", 2.0)
        assertEquals("no_result", l.toMap()["verdict"])
        assertEquals("warning" to "nothing holds the part", l.last())
        assertEquals("no_result", EngineeringCase.reportLines("not json", 2.0).toMap()["verdict"])
    }
}
