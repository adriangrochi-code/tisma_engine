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
            out=""; summary=""
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
    fun `the simple settings are the engine's keys (docs CONFIG md)`() {
        assertEquals("""{"layers":{"height":0.12},"support":{"enabled":true}}""",
            EngineRunner.SimpleSettings(0.12, true).toJson().toString())
    }
}
