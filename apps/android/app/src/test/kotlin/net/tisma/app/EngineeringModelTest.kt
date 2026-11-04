package net.tisma.app

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.doubleOrNull
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import net.tisma.app.engine.EngineRunner
import net.tisma.app.engine.EngineeringCase
import net.tisma.app.engine.MeshFile
import net.tisma.app.engine.Picking
import org.junit.Assume.assumeTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File
import kotlin.math.abs
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

/** Engineering on a bare model: faces picked by touch, the case for the engine (mobile_app.md, rule 13; MA6). */
class EngineeringModelTest {
    @get:Rule
    val tmp = TemporaryFolder()

    /** A box as an ASCII STL, two triangles per side. */
    private fun box(x: Float, y: Float, z: Float): File {
        val c = listOf(floatArrayOf(0f, 0f, 0f), floatArrayOf(x, 0f, 0f), floatArrayOf(x, y, 0f), floatArrayOf(0f, y, 0f),
            floatArrayOf(0f, 0f, z), floatArrayOf(x, 0f, z), floatArrayOf(x, y, z), floatArrayOf(0f, y, z))
        val faces = listOf(intArrayOf(0, 2, 1), intArrayOf(0, 3, 2), intArrayOf(4, 5, 6), intArrayOf(4, 6, 7),
            intArrayOf(0, 1, 5), intArrayOf(0, 5, 4), intArrayOf(1, 2, 6), intArrayOf(1, 6, 5),
            intArrayOf(2, 3, 7), intArrayOf(2, 7, 6), intArrayOf(3, 0, 4), intArrayOf(3, 4, 7))
        val text = buildString {
            appendLine("solid box")
            for (f in faces) {
                appendLine(" facet normal 0 0 0\n  outer loop")
                for (i in f) appendLine("   vertex ${c[i][0]} ${c[i][1]} ${c[i][2]}")
                appendLine("  endloop\n endfacet")
            }
            appendLine("endsolid box")
        }
        return tmp.newFile("beam.stl").apply { writeText(text) }
    }

    private fun pick(mesh: MeshFile, o: FloatArray, d: FloatArray, kind: EngineeringCase.Kind, useFaces: Boolean = true): EngineeringCase.Condition {
        val t = assertNotNull(Picking.hit(mesh, o, d))
        val face = Picking.flatFace(mesh, t)
        val n = Picking.normal(mesh, face)
        return EngineeringCase.Condition(kind, face, Picking.box(mesh, face, EngineeringCase.pad("normal")),
            direction = floatArrayOf(-n[0], -n[1], -n[2]), face = if (useFaces) EngineeringCase.endFace(mesh, face, n) else null)
    }

    @Test
    fun `a touched face is its flat side, boxed, and the load pushes into it`() {
        val mesh = MeshFile.read(box(100f, 10f, 10f)).onBed(110f, 110f)
        val top = pick(mesh, floatArrayOf(110f, 110f, 100f), floatArrayOf(0f, 0f, -1f), EngineeringCase.Kind.LOAD)
        assertEquals(2, top.triangles.size)
        assertEquals(10.08f, top.box[5], 1e-4f)
        assertEquals(9.92f, top.box[2], 1e-4f)
        assertEquals("top", top.face)
        assertEquals(-1f, top.direction[2], 1e-5f)
        val end = pick(mesh, floatArrayOf(0f, 110f, 5f), floatArrayOf(1f, 0f, 0f), EngineeringCase.Kind.FIXED, useFaces = false)
        assertEquals(2, end.triangles.size)
        // Back in the file's coordinates: the end at x = 0.
        val j = EngineeringCase(listOf(end, top)).toJson(mesh.offset)
        val x = j["fixtures"]!!.jsonArray[0].jsonObject["box"]!!.jsonObject["min"]!!.jsonArray[0].jsonPrimitive.doubleOrNull!!
        assertEquals(-0.08, x, 1e-4)
        assertEquals("\"top\"", j["loads"]!!.jsonArray[0].jsonObject["face"].toString())
    }

    /**
     * MA6 with the real engine, when TISMA_HOST_ENGINE names a host build: a
     * beam held at both ends with 200 N on top gives the same lowest safety
     * marked by touch as with the engine's own faces.
     */
    @Test
    fun `MA6 a supported beam with a load gives the engine's verdict`() {
        val exe = System.getenv("TISMA_HOST_ENGINE")?.let(::File)
        assumeTrue("TISMA_HOST_ENGINE not set", exe != null && exe.canExecute())
        val stl = box(60f, 10f, 6f)
        val mesh = MeshFile.read(stl).onBed(110f, 110f)
        fun touched(useFaces: Boolean): Double {
            val case = EngineeringCase(listOf(
                pick(mesh, floatArrayOf(0f, 110f, 3f), floatArrayOf(1f, 0f, 0f), EngineeringCase.Kind.FIXED, useFaces),
                pick(mesh, floatArrayOf(300f, 110f, 3f), floatArrayOf(-1f, 0f, 0f), EngineeringCase.Kind.FIXED, useFaces),
                pick(mesh, floatArrayOf(110f, 110f, 100f), floatArrayOf(0f, 0f, -1f), EngineeringCase.Kind.LOAD, useFaces).copy(newtons = 200.0),
            ), quality = "normal")
            val runner = EngineRunner(exe!!, tmp.newFolder())
            val json = JsonObject(case.toJson(mesh.offset) + mapOf("model" to JsonPrimitive("solid"), "resolution_check" to JsonPrimitive(false)))
            val r = runner.analyzeModel(stl, EngineRunner.SimpleSettings(), json, null, {}, { false })
            return r.summary["min_safety"]!!.jsonPrimitive.doubleOrNull!!
        }
        val byFace = touched(true)
        val byBox = touched(false)
        // The same with the engine's faces.
        val faces = tmp.newFile("faces.json").apply {
            writeText("""{"material":"bambu-pla-basic","quality":"normal","model":"solid","resolution_check":false,
                "fixtures":[{"face":"x_min"},{"face":"x_max"}],"loads":[{"face":"top","force":[0,0,-200]}]}""")
        }
        val config = tmp.newFile("c.json").apply { writeText(EngineRunner.SimpleSettings().toJson().toString()) }
        val out = File(tmp.root, "r.json")
        val p = ProcessBuilder(exe!!.path, "analyze", stl.path, "--config", config.path, "--case", faces.path, "-o", out.path)
            .redirectErrorStream(true).start()
        p.inputStream.readBytes(); p.waitFor()
        val ref = Json.parseToJsonElement(out.readText()).jsonObject["result"]!!.jsonObject["min_safety"]!!.jsonPrimitive.doubleOrNull!!
        println("MA6 end faces $byFace, boxes $byBox, the engine's faces $ref")
        assertEquals(ref, byFace, 1e-9)
        assertTrue(abs(byBox - ref) / ref < 0.02, "boxes $byBox, faces $ref")
    }
}
