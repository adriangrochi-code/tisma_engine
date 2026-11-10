package net.tisma.app

import android.content.Context
import androidx.compose.ui.test.assertIsEnabled
import androidx.compose.ui.test.assertIsNotEnabled
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performTextReplacement
import androidx.test.core.app.ApplicationProvider
import net.tisma.app.cloud.CloudViewModel
import net.tisma.app.data.AppSettings
import net.tisma.app.data.KeystoreSecretStore
import net.tisma.app.engine.EngineRunner
import net.tisma.app.engine.EngineeringCase
import net.tisma.app.engine.SliceViewModel
import net.tisma.app.printers.PrinterStore
import net.tisma.app.printers.PrintersViewModel
import net.tisma.app.ui.Navigator
import net.tisma.app.ui.TismaApp
import net.tisma.app.ui.TismaTheme
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.io.File
import kotlin.test.assertEquals

/** Engineering on a bare model on the Plate: what a tap marks, the marked faces, Analyze (mobile_app.md, rule 13). */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35], qualifiers = "w411dp-h891dp-xxhdpi")
class EngineeringScreenTest {
    @get:Rule
    val compose = createComposeRule()

    @Test
    fun `faces are marked by taps and the analysis waits for a fixed face and a load`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val settings = AppSettings(context)
        val work = File(context.cacheDir, "engine")
        // An engine that "is there" (the card needs one); it is not run here.
        val exe = File(work, "fake").apply { parentFile!!.mkdirs(); writeText("#!/bin/sh\nexit 1\n"); setExecutable(true) }
        val slice = SliceViewModel(EngineRunner(exe, work), work, saved = settings)
        val cloud = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5)
        val printers = PrintersViewModel(PrinterStore(context, MemorySecrets()))
        val nav = Navigator(net.tisma.app.ui.Section.ENGINEERING)
        compose.setContent { TismaTheme { TismaApp(cloud, printers, slice, settings, nav) } }
        compose.runOnIdle { slice.open("cube.stl") { File("../../../tests/data/cube_ascii.stl").inputStream() } }
        wait("model_engineering")
        compose.onNodeWithTag("eng_analyze_model").performScrollTo().assertIsNotEnabled()
        compose.onNodeWithTag("eng_mark_fixed").performScrollTo().performClick()
        compose.runOnIdle { assertEquals(EngineeringCase.Kind.FIXED, slice.state.value.marking) }
        val b = slice.state.value.mesh!!.bounds
        val cx = (b[0] + b[3]) / 2; val cy = (b[1] + b[4]) / 2
        // From below, the bottom face; then a load from above on the top face.
        compose.runOnIdle { slice.pick(floatArrayOf(cx, cy, -50f), floatArrayOf(0f, 0f, 1f)) }
        wait("eng_condition_0")
        compose.onNodeWithTag("eng_mark_load").performScrollTo().performClick()
        compose.runOnIdle { slice.pick(floatArrayOf(cx, cy, 50f), floatArrayOf(0f, 0f, -1f)) }
        wait("eng_condition_1")
        compose.runOnIdle {
            val c = slice.state.value.case.conditions
            assertEquals(listOf("bottom", "top"), c.map { it.face })
            assertEquals(-1f, c[1].direction[2], 1e-5f)
        }
        compose.onNodeWithTag("eng_analyze_model").performScrollTo().assertIsEnabled()
        // The top face marked again as fixed replaces the load; tapped once more, it is taken off.
        compose.onNodeWithTag("eng_mark_fixed").performScrollTo().performClick()
        compose.runOnIdle { slice.pick(floatArrayOf(cx, cy, 50f), floatArrayOf(0f, 0f, -1f)) }
        compose.waitUntil(5_000) { org.robolectric.shadows.ShadowLooper.idleMainLooper(); slice.state.value.case.conditions.map { it.kind } == listOf(EngineeringCase.Kind.FIXED, EngineeringCase.Kind.FIXED) }
        compose.runOnIdle { slice.pick(floatArrayOf(cx, cy, 50f), floatArrayOf(0f, 0f, -1f)) }
        compose.waitUntil(5_000) { org.robolectric.shadows.ShadowLooper.idleMainLooper(); slice.state.value.case.conditions.size == 1 }
        compose.onNodeWithTag("eng_analyze_model").performScrollTo().assertIsNotEnabled()
        // Rule 13: the temperature, the profile's nozzle temperature that the engine uses for layer adhesion.
        compose.onNodeWithTag("eng_temperature").performScrollTo().performTextReplacement("235")
        compose.runOnIdle {
            assertEquals(235.0, (slice.state.value.value("machine.variables.temperature") as kotlinx.serialization.json.JsonPrimitive).content.toDouble())
        }
    }

    @Test
    fun `the plate's tools turn and scale the shown model`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val settings = AppSettings(context)
        val work = File(context.cacheDir, "engine")
        val slice = SliceViewModel(EngineRunner(File(work, "none"), work), work, saved = settings)
        val cloud = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5)
        val printers = PrintersViewModel(PrinterStore(context, MemorySecrets()))
        compose.setContent { TismaTheme { TismaApp(cloud, printers, slice, settings, Navigator(net.tisma.app.ui.Section.PLATE)) } }
        compose.runOnIdle { slice.open("cube.stl") { File("../../../tests/data/cube_ascii.stl").inputStream() } }
        wait("transform")
        compose.onNodeWithTag("scale").performScrollTo().performTextReplacement("200")
        compose.waitUntil(5_000) {
            org.robolectric.shadows.ShadowLooper.idleMainLooper()
            slice.state.value.mesh?.bounds?.let { it[5] - it[2] }?.let { kotlin.math.abs(it - 20f) < 1e-3f } == true
        }
        compose.onNodeWithTag("rotate_X").performScrollTo().performClick()
        compose.onNodeWithTag("transform_reset").performScrollTo().performClick()
        compose.waitUntil(5_000) {
            org.robolectric.shadows.ShadowLooper.idleMainLooper()
            slice.state.value.mesh?.bounds?.let { it[5] - it[2] }?.let { kotlin.math.abs(it - 10f) < 1e-3f } == true
        }
    }

    @Test
    fun `a long analysis is said before running, and runs when asked`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val settings = AppSettings(context)
        val work = File(context.cacheDir, "engine").apply { mkdirs() }
        // An engine whose estimate says 100 s (400 s on a phone); the run itself writes a valid report.
        val exe = File(work, "slow").apply {
            writeText("""#!/bin/sh
                |out=""; est=0
                |while [ ${'$'}# -gt 0 ]; do case "${'$'}1" in -o) out="${'$'}2"; shift;; --estimate) est=1;; esac; shift; done
                |if [ ${'$'}est = 1 ]; then echo '{"estimate":true,"estimated_seconds":100,"estimated_memory_mb":50}' > "${'$'}out"
                |else echo "progress: solving 0.5"; echo '{"valid":true,"result":{"min_safety":3.0},"warnings":[]}' > "${'$'}out"; fi
                |""".trimMargin())
            setExecutable(true)
        }
        val slice = SliceViewModel(EngineRunner(exe, work), work, saved = settings)
        val cloud = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5)
        val printers = PrintersViewModel(PrinterStore(context, MemorySecrets()))
        compose.setContent { TismaTheme { TismaApp(cloud, printers, slice, settings, Navigator(net.tisma.app.ui.Section.ENGINEERING)) } }
        compose.runOnIdle { slice.open("cube.stl") { File("../../../tests/data/cube_ascii.stl").inputStream() } }
        wait("model_engineering")
        val b = slice.state.value.mesh!!.bounds
        val cx = (b[0] + b[3]) / 2; val cy = (b[1] + b[4]) / 2
        compose.runOnIdle { slice.setMarking(EngineeringCase.Kind.FIXED); slice.pick(floatArrayOf(cx, cy, -50f), floatArrayOf(0f, 0f, 1f)) }
        compose.waitUntil(5_000) { org.robolectric.shadows.ShadowLooper.idleMainLooper(); slice.state.value.case.conditions.size == 1 }
        compose.runOnIdle { slice.setMarking(EngineeringCase.Kind.LOAD); slice.pick(floatArrayOf(cx, cy, 50f), floatArrayOf(0f, 0f, -1f)) }
        compose.waitUntil(5_000) { org.robolectric.shadows.ShadowLooper.idleMainLooper(); slice.state.value.case.conditions.size == 2 }
        compose.onNodeWithTag("eng_analyze_model").performScrollTo().performClick()
        wait("eng_slow")
        compose.onNodeWithTag("eng_run_anyway").performScrollTo().performClick()
        compose.waitUntil(10_000) {
            org.robolectric.shadows.ShadowLooper.idleMainLooper()
            slice.state.value.analysis?.any { it == ("verdict" to "holds") } == true
        }
    }

    private fun wait(tag: String) = compose.waitUntil(10_000) {
        org.robolectric.shadows.ShadowLooper.idleMainLooper()
        compose.onAllNodes(hasTestTag(tag), useUnmergedTree = true).fetchSemanticsNodes().isNotEmpty()
    }
}
