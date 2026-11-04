package net.tisma.app

import net.tisma.app.engine.MeshFile
import org.junit.Assume.assumeTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** The plate's tools on a bare model: turns, mirror, scale and lay on a face, and the STL the engine reads. */
class TransformTest {
    @get:Rule
    val tmp = TemporaryFolder()

    private val cube = File(System.getProperty("user.dir")).parentFile.parentFile.parentFile.resolve("tests/data/cube_ascii.stl")

    /** A 10 × 20 × 30 box from the 10 mm cube. */
    private fun box() = MeshFile.read(cube).transformed(floatArrayOf(1f, 0f, 0f, 0f, 2f, 0f, 0f, 0f, 3f))

    private fun size(m: MeshFile) = m.bounds.let { b -> listOf(b[3] - b[0], b[4] - b[1], b[5] - b[2]) }

    /** Six times the signed volume: positive when the faces look out. */
    private fun volume(m: MeshFile): Double {
        var v = 0.0
        val p = m.positions
        for (t in 0 until m.triangles) {
            val i = 9 * t
            v += p[i] * (p[i + 4] * p[i + 8] - p[i + 5] * p[i + 7]) - p[i + 1] * (p[i + 3] * p[i + 8] - p[i + 5] * p[i + 6]) +
                p[i + 2] * (p[i + 3] * p[i + 7] - p[i + 4] * p[i + 6]).toDouble()
        }
        return v / 6
    }

    @Test
    fun `turns, mirror and scale change the sizes and keep the faces looking out`() {
        val b = box()
        assertEquals(listOf(10f, 20f, 30f), size(b))
        assertEquals(listOf(10f, 30f, 20f), size(b.transformed(MeshFile.quarterTurn(0))))
        assertEquals(listOf(30f, 20f, 10f), size(b.transformed(MeshFile.quarterTurn(1))))
        assertEquals(listOf(20f, 10f, 30f), size(b.transformed(MeshFile.quarterTurn(2))))
        assertEquals(6000.0, volume(b), 1e-2)
        assertEquals(6000.0, volume(b.transformed(MeshFile.mirror(0))), 1e-2)
        assertEquals(6000.0 * 8, volume(b.transformed(MeshFile.scale(2f))), 1e-1)
    }

    @Test
    fun `lay on a face turns its normal down`() {
        for (n in listOf(floatArrayOf(0f, 0f, 1f), floatArrayOf(0f, 0f, -1f), floatArrayOf(1f, 0f, 0f), floatArrayOf(0.6f, 0f, 0.8f))) {
            val r = MeshFile.layFlat(n)
            val down = floatArrayOf(r[0] * n[0] + r[1] * n[1] + r[2] * n[2], r[3] * n[0] + r[4] * n[1] + r[5] * n[2],
                r[6] * n[0] + r[7] * n[1] + r[8] * n[2])
            assertEquals(0f, down[0], 1e-5f); assertEquals(0f, down[1], 1e-5f); assertEquals(-1f, down[2], 1e-5f)
        }
    }

    @Test
    fun `the written STL reads back the same, and the engine slices it`() {
        val m = box().transformed(MeshFile.quarterTurn(0))
        val f = tmp.newFile("t.stl")
        m.writeStl(f)
        val back = MeshFile.read(f)
        assertEquals(m.triangles, back.triangles)
        assertEquals(size(m), size(back))
        val exe = System.getenv("TISMA_HOST_ENGINE")?.let(::File)
        assumeTrue(exe != null && exe.canExecute())
        val cfg = tmp.newFile("c.json").apply { writeText("{}") }
        val out = File(tmp.root, "t.gcode")
        val p = ProcessBuilder(exe!!.path, "slice", f.path, "--config", cfg.path, "-o", out.path).redirectErrorStream(true).start()
        val log = p.inputStream.bufferedReader().readText()
        assertEquals(0, p.waitFor(), log)
        // Turned about X, the 30 mm height became 20 mm: 100 layers of 0.2 mm.
        assertTrue(out.readLines().count { it.startsWith(";LAYER_CHANGE") } == 100, log.takeLast(300))
    }
}
