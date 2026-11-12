import kotlinx.serialization.json.Json
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import net.tisma.app.engine.PrintChanges
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse

/** The phone's Print and Material changes over a printer's profile. */
class PrintChangesTest {
    @Test
    fun changesGoOverTheProfileAndKeepTheRest() {
        val profile = """{"layers":{"height":0.2,"first_height":0.3},"machine":{"nozzle_diameter":0.4,"variables":{"temperature":215,"bed_temperature":60}}}"""
        val out = Json.parseToJsonElement(PrintChanges(layerHeight = 0.12, infillPercent = 35.0, supports = true, nozzleTemp = 230.0).over(profile)).jsonObject
        val layers = out["layers"]!!.jsonObject
        assertEquals("0.12", layers["height"]!!.jsonPrimitive.content)
        assertEquals("0.3", layers["first_height"]!!.jsonPrimitive.content)
        assertEquals("35.0", out["infill"]!!.jsonObject["density"]!!.jsonPrimitive.content)
        val vars = out["machine"]!!.jsonObject["variables"]!!.jsonObject
        assertEquals("230.0", vars["temperature"]!!.jsonPrimitive.content)
        assertEquals("60", vars["bed_temperature"]!!.jsonPrimitive.content, "a temperature not changed is the filament's")
        assertEquals("0.4", out["machine"]!!.jsonObject["nozzle_diameter"]!!.jsonPrimitive.content)
    }

    @Test
    fun noTemperatureChangeLeavesMachineAlone() {
        assertFalse(PrintChanges().toJson().containsKey("machine"))
    }
}
