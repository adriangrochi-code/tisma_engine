package net.tisma.app

import net.tisma.app.engine.EngineRunner
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File
import java.util.concurrent.atomic.AtomicBoolean
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/**
 * The runner against a fake engine (a shell script that speaks the command
 * line of cloud.md, rule 18). The real engine cannot run here: it is built
 * for Android (bionic), and this machine has no emulator.
 */
class EngineRunnerTest {
    @get:Rule
    val tmp = TemporaryFolder()

    private fun fake(body: String): File = tmp.newFile("fake_engine.sh").apply {
        writeText("#!/bin/sh\n" + """
            cmd="${'$'}1"; out=""; summary=""
            while [ ${'$'}# -gt 0 ]; do
              case "${'$'}1" in
                -o) out="${'$'}2"; shift ;;
                --summary) summary="${'$'}2"; shift ;;
              esac
              shift
            done
        """.trimIndent() + "\n" + body)
        setExecutable(true)
    }

    private fun project() = tmp.newFile("cube.3mf").apply { writeText("zip") }

    @Test
    fun `progress lines, the G-code and the summary of rule 18`() {
        val engine = fake("""
            for p in 0 25 50 100; do echo "progress ${'$'}p"; done
            echo "G28" > "${'$'}out"
            echo '{"print_time": "18m 42s", "print_time_s": 1122, "filament_mm": 3215.4, "filament_g": 9.59, "layers": 100}' > "${'$'}summary"
        """.trimIndent())
        val seen = mutableListOf<Int>()
        val r = EngineRunner(engine, tmp.newFolder("work")).sliceProject(project(), { seen += it }, { false })
        assertEquals(listOf(0, 25, 50, 100), seen)
        assertEquals("G28\n", r.gcode.readText())
        assertEquals("18m 42s", r.printTime)
        assertEquals(1122.0, r.printTimeS)
        assertEquals(9.59, r.filamentG)
        assertEquals(100, r.layers)
    }

    @Test
    fun `a failure carries the summary's error, code and error_es (rule 19a)`() {
        val engine = fake("""
            echo '{"error": "Part 1 is outside the bed", "code": "error", "error_es": "La pieza 1 queda fuera de la cama"}' > "${'$'}summary"
            exit 2
        """.trimIndent())
        val e = assertFailsWith<EngineRunner.EngineException> {
            EngineRunner(engine, tmp.newFolder("work")).sliceProject(project(), {}, { false })
        }
        assertEquals("Part 1 is outside the bed", e.error)
        assertEquals("La pieza 1 queda fuera de la cama", e.errorEs)
        assertEquals(2, e.exitCode)
    }

    @Test
    fun `without a summary, the last line of the error output is the reason`() {
        val engine = fake("echo 'error: slice needs a model' >&2; exit 1")
        val e = assertFailsWith<EngineRunner.EngineException> {
            EngineRunner(engine, tmp.newFolder("work")).sliceModel(project(), EngineRunner.SimpleSettings(), {}, { false })
        }
        assertEquals("error: slice needs a model", e.error)
    }

    @Test
    fun `MA3 cancelling ends the engine at once and leaves no G-code`() {
        val engine = fake("""
            echo "G28" > "${'$'}out"
            i=0; while [ ${'$'}i -lt 600 ]; do echo "progress ${'$'}((i / 6))"; sleep 0.1; i=${'$'}((i + 1)); done
        """.trimIndent())
        val cancel = AtomicBoolean(false)
        val work = tmp.newFolder("work")
        val started = System.currentTimeMillis()
        assertFailsWith<EngineRunner.Cancelled> {
            EngineRunner(engine, work).sliceProject(project(), { if (it >= 1) cancel.set(true) }, { cancel.get() })
        }
        assertTrue(System.currentTimeMillis() - started < 10_000)
        assertFalse(File(work, "cube.gcode").exists())
    }

    @Test
    fun `a model gets the same summary as a project`() {
        val engine = fake("""
            echo "G28" > "${'$'}out"
            echo '{"print_time": "5m", "print_time_s": 300, "filament_mm": 1000, "layers": 50}' > "${'$'}summary"
        """.trimIndent())
        val r = EngineRunner(engine, tmp.newFolder("work")).sliceModel(project(), EngineRunner.SimpleSettings(), {}, { false })
        assertEquals(50, r.layers)
        assertEquals(1000.0, r.filamentMm)
        assertEquals(null, r.filamentG)
    }

    @Test
    fun `the engine's profile is the base and the simple choices go on top`() {
        val engine = fake("""
            if [ "${'$'}cmd" = "profile" ]; then
              echo '{"machine":{"nozzle_diameter":0.6},"layers":{"height":0.3,"first_height":0.3}}' > "${'$'}out"
              exit 0
            fi
            echo "G28" > "${'$'}out"; echo '{"layers": 1}' > "${'$'}summary"
        """.trimIndent())
        val work = File(tmp.root, "work")
        val r = EngineRunner(engine, work).sliceModel(project(), EngineRunner.SimpleSettings(0.12, false), {}, { false },
            EngineRunner.ProfileChoice("AnkerMake M5", 0.6, null))
        assertTrue(r.profileUsed)
        assertEquals("""{"machine":{"nozzle_diameter":0.6},"layers":{"height":0.12,"first_height":0.3},"support":{"enabled":false}}""",
            File(work, "settings.json").readText())
    }

    @Test
    fun `an engine without the profile command slices with its defaults and says so`() {
        val engine = fake("""
            if [ "${'$'}cmd" = "profile" ]; then echo "usage" >&2; exit 1; fi
            echo "G28" > "${'$'}out"; echo '{"layers": 1}' > "${'$'}summary"
        """.trimIndent())
        val r = EngineRunner(engine, tmp.newFolder("work")).sliceModel(project(), EngineRunner.SimpleSettings(), {}, { false },
            EngineRunner.ProfileChoice("Nope", 0.4, null))
        assertFalse(r.profileUsed)
    }

    @Test
    fun `the catalog of the engine is read`() {
        val root = File(System.getProperty("user.dir")).parentFile.parentFile.parentFile
        val c = net.tisma.app.engine.Catalog.parse(File(root, "resources/catalog/printers.json").readText(),
            File(root, "resources/catalog/filaments.json").readText())
        assertTrue(c.printers.size > 100)
        val m5 = c.search("anker m5").first()
        assertEquals("AnkerMake M5", m5.name)
        assertEquals(0.4, m5.nozzles.first())
        assertTrue(c.filamentsFor(m5).all { it.diameter == 0.0 || it.diameter == 1.75 })
    }

    @Test
    fun `analyze runs the nodes' command line and returns the verdict`() {
        val engine = fake("""
            echo "progress 50"
            echo "{}" > "${'$'}out"
            echo '{"verdict": "holds", "min_safety": 3.2, "required_safety": 2, "mass_g": 12.5}' > "${'$'}summary"
        """.trimIndent())
        val r = EngineRunner(engine, tmp.newFolder("work")).analyze(project(), 0, "normal", {}, { false })
        assertEquals("\"holds\"", r.summary["verdict"].toString())
        assertTrue(r.result.isFile)
    }

    @Test
    fun `rule 13 the lightest infill runs the lightest-infill mode without zones`() {
        val engine = tmp.newFile("args_engine.sh").apply {
            writeText("#!/bin/sh\n" + """
                all="${'$'}*"; out=""; summary=""
                while [ ${'$'}# -gt 0 ]; do
                  case "${'$'}1" in -o) out="${'$'}2"; shift ;; --summary) summary="${'$'}2"; shift ;; esac
                  shift
                done
                echo "{}" > "${'$'}out"
                echo "{\"mode\": \"lightest-infill\", \"found\": true, \"density\": 15, \"args\": \"${'$'}all\"}" > "${'$'}summary"
            """.trimIndent())
            setExecutable(true)
        }
        val r = EngineRunner(engine, tmp.newFolder("work")).lightestInfill(project(), 1, "fast", {}, { false })
        val args = r.summary["args"].toString()
        assertTrue("--mode lightest-infill --no-zones --object 1" in args, args)
        assertEquals("15", r.summary["density"].toString())
    }

    @Test
    fun `a project slice takes the panel's changes with --config-over`() {
        val engine = tmp.newFile("over_engine.sh").apply {
            writeText("#!/bin/sh\n" + """
                out=""; summary=""; over=""
                while [ ${'$'}# -gt 0 ]; do
                  case "${'$'}1" in -o) out="${'$'}2"; shift ;; --summary) summary="${'$'}2"; shift ;; --config-over) over="${'$'}2"; shift ;; esac
                  shift
                done
                cat "${'$'}over" > "${'$'}out"
                echo "{}" > "${'$'}summary"
            """.trimIndent())
            setExecutable(true)
        }
        val runner = EngineRunner(engine, tmp.newFolder("work"))
        val r = runner.sliceProject(project(), {}, { false }, mapOf("infill.density" to kotlinx.serialization.json.JsonPrimitive(15)))
        assertEquals("{\"infill\":{\"density\":15}}", r.gcode.readText().trim())
    }

    @Test
    fun `copies go to the engine as --copies`() {
        val engine = tmp.newFile("copies_engine.sh").apply {
            writeText("#!/bin/sh\n" + """
                all="${'$'}*"; out=""; summary=""
                while [ ${'$'}# -gt 0 ]; do
                  case "${'$'}1" in -o) out="${'$'}2"; shift ;; --summary) summary="${'$'}2"; shift ;; esac
                  shift
                done
                echo "${'$'}all" > "${'$'}out"; echo "{}" > "${'$'}summary"
            """.trimIndent())
            setExecutable(true)
        }
        val model = tmp.newFile("m.stl").apply { writeText("solid x\nendsolid x\n") }
        val r = EngineRunner(engine, tmp.newFolder("work")).sliceModel(model, EngineRunner.SimpleSettings(copies = 4), {}, { false })
        assertTrue(r.gcode.readText().contains("--copies 4"), r.gcode.readText())
    }

    @Test
    fun `MA9 the triangles of an STL and a 3MF are counted for the memory estimate`() {
        val root = File(System.getProperty("user.dir")).parentFile.parentFile.parentFile
        val cube = File(root, "tests/data/cube_ascii.stl")
        assertEquals(12, net.tisma.app.engine.MemoryEstimate.triangles(cube))
        val bin = tmp.newFile("b.stl").apply {
            writeBytes(ByteArray(80) + byteArrayOf(2, 0, 0, 0) + ByteArray(100))
        }
        assertEquals(2, net.tisma.app.engine.MemoryEstimate.triangles(bin))
        val zip = tmp.newFile("m.3mf")
        java.util.zip.ZipOutputStream(zip.outputStream()).use { z ->
            z.putNextEntry(java.util.zip.ZipEntry("3D/3dmodel.model"))
            z.write("<triangles>".plus("<triangle v1=\"0\" v2=\"1\" v3=\"2\"/>".repeat(5000)).plus("</triangles>").toByteArray())
        }
        assertEquals(5000, net.tisma.app.engine.MemoryEstimate.triangles(zip))
    }

    @Test
    fun `the simple settings are the engine's keys (docs CONFIG md)`() {
        assertEquals("""{"layers":{"height":0.12},"support":{"enabled":true}}""",
            EngineRunner.SimpleSettings(0.12, true).toJson().toString())
    }
}
