package net.tisma.app

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import net.tisma.app.data.AppSettings
import net.tisma.app.engine.EngineRunner
import net.tisma.app.engine.SliceViewModel
import org.junit.Assume.assumeTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.io.File
import kotlin.math.abs
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

/**
 * Design parts, calibrations, orientation and aerodynamics on the phone, with
 * the real engine (TISMA_HOST_ENGINE names a host build; skipped without it).
 */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class DesignToolsTest {
    @get:Rule
    val tmp = TemporaryFolder()

    private val exe = System.getenv("TISMA_HOST_ENGINE")?.let(::File)
    private val cube = File("../../../tests/data/cube_ascii.stl")

    private fun vm(): Pair<SliceViewModel, EngineRunner> {
        assumeTrue(exe != null && exe.canExecute())
        val work = tmp.newFolder("work")
        val runner = EngineRunner(exe!!, work)
        val context: Context = ApplicationProvider.getApplicationContext()
        return SliceViewModel(runner, work, saved = AppSettings(context)) to runner
    }

    private fun idle(until: () -> Boolean) = eventually(60_000) { org.robolectric.shadows.ShadowLooper.idleMainLooper(); until() }

    @Test
    fun `gears and fasteners come from the engine, and a refused spec says why`() {
        val (slice, runner) = vm()
        val gear = runner.partDefaults("gear")
        val (stl, sizes) = runner.makePart("gear", gear, "gear")
        assertTrue(stl.length() > 84)
        assertEquals("30.0", sizes["pitch_diameter"].toString())
        val nut = JsonObject(runner.partDefaults("fastener") + mapOf("kind" to JsonPrimitive("hex_nut"), "size" to JsonPrimitive("M3")))
        assertTrue(runner.makePart("fastener", nut, "nut").first.length() > 84)
        var error: String? = "none"
        slice.makePart("gear", JsonObject(gear + mapOf("teeth" to JsonPrimitive(2)))) { error = it }
        idle { error != "none" }
        assertTrue(error!!.contains("teeth"), error)
        // A good one goes to the plate.
        error = "none"
        slice.makePart("gear", gear) { error = it }
        idle { error == null && slice.state.value.mesh != null }
        assertEquals("gear.stl", slice.state.value.modelName)
    }

    @Test
    fun `a temperature tower is sliced with its settings and the best value goes to the panel`() {
        val (slice, _) = vm()
        var error: String? = "none"
        slice.calibrate("temperature", null) { error = it }
        idle { error != "none" && slice.state.value.calibration != null }
        val c = slice.state.value.calibration!!
        assertTrue(c.segments.size >= 3, c.toString())
        idle { slice.state.value.mesh != null }
        slice.slice(checkMemory = false)
        idle { !slice.state.value.slicing && (slice.state.value.result != null || slice.state.value.error != null) }
        val r = assertNotNull(slice.state.value.result, slice.state.value.error?.error)
        // The tower's height changes are in the G-code: more than one nozzle temperature.
        val temps = r.gcode.readLines().filter { it.startsWith("M104") || it.startsWith("M109") }.map { it.substringAfter('S').substringBefore(' ') }.toSet()
        assertTrue(temps.size >= 3, temps.toString())
        val best = c.segments[1].third
        slice.applyCalibration(best)
        idle { slice.state.value.calibration == null }
        val changes = slice.state.value.settings.changes
        assertTrue(changes.values.any { it.toString().trim('"').toDoubleOrNull() == best }, changes.toString())
    }

    @Test
    fun `orientation and drag run on a bare model`() {
        val (slice, runner) = vm()
        val (rot, o) = runner.orientModel(cube, EngineRunner.SimpleSettings(), null, {}, { false })
        // A rotation: its rows are unit vectors.
        for (r in 0..2) assertEquals(1f, rot[3 * r] * rot[3 * r] + rot[3 * r + 1] * rot[3 * r + 1] + rot[3 * r + 2] * rot[3 * r + 2], 1e-4f)
        assertEquals("tisma-orientation", (o["format"] as JsonPrimitive).content)
        val a = runner.aeroModel(cube, EngineRunner.SimpleSettings(), null, "fast", 10.0, "x", {}, { false })
        val cd = (a.summary["cd"] as JsonPrimitive).content.toDouble()
        assertTrue(abs(cd - 1.18) < 0.2, "cd $cd")
        // Through the plate.
        slice.open("cube.stl") { cube.inputStream() }
        idle { slice.state.value.mesh != null }
        slice.orientAutomatically()
        idle { !slice.state.value.analyzing }
        assertEquals(null, slice.state.value.error)
    }
}
