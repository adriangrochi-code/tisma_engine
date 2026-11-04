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
    fun `the simple settings are the engine's keys (docs CONFIG md)`() {
        assertEquals("""{"layers":{"height":0.12},"support":{"enabled":true}}""",
            EngineRunner.SimpleSettings(0.12, true).toJson().toString())
    }
}
