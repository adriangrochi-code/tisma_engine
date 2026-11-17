package net.tisma.printers

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.boolean
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import net.tisma.mockprinters.MockBambu
import org.junit.After
import org.junit.Test
import java.io.File
import kotlin.test.assertEquals

/**
 * printer_connections.md, BL2: the `project_file` the app sends is the one the desktop sends, field by field, for the
 * shared reference in tests/data/bambu_project_file.json (a G-code, its name and the print options).
 */
class BambuProjectFileTest {
    private val printer = MockBambu().start()

    @After
    fun stop() = printer.stop()

    @Test
    fun `BL2 the project_file matches the shared reference field by field`() {
        val ref = Json.parseToJsonElement(File("../../../tests/data/bambu_project_file.json").readText()).jsonObject
        val gcode = ref["gcode"]!!.jsonPrimitive.content.toByteArray()
        val name = ref["name"]!!.jsonPrimitive.content
        val o = ref["options"]!!.jsonObject
        fun flag(k: String) = o[k]!!.jsonPrimitive.boolean
        val options = PrintOptions(bedLeveling = flag("bed_levelling"), flowCalibration = flag("flow_cali"),
            vibrationCalibration = flag("vibration_cali"), timelapse = flag("timelapse"))
        BambuConnection.of(printer.url, printer.accessCode, null)
            .upload("$name.gcode", gcode.size.toLong(), start = true, options) { gcode.inputStream() }
        val want = ref["project_file"]!!.jsonObject["print"]!!.jsonObject
        fun isProjectFile(c: JsonObject) = c["command"]?.jsonPrimitive?.content == "project_file"
        val deadline = System.currentTimeMillis() + 10_000
        while (printer.commands.none(::isProjectFile) && System.currentTimeMillis() < deadline) Thread.sleep(20)
        val sent: JsonObject = printer.commands.single(::isProjectFile)
        assertEquals(want.keys.sorted(), sent.keys.sorted(), "the same fields")
        for (k in want.keys) assertEquals(want[k], sent[k], "field $k")
    }
}
