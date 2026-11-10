package net.tisma.app

import android.content.Context
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.test.core.app.ApplicationProvider
import net.tisma.app.cloud.CloudViewModel
import net.tisma.app.data.AppSettings
import net.tisma.app.data.KeystoreSecretStore
import net.tisma.app.engine.EngineRunner
import net.tisma.app.engine.SliceViewModel
import net.tisma.app.printers.PrinterStore
import net.tisma.app.printers.PrintersViewModel
import net.tisma.app.ui.Navigator
import net.tisma.app.ui.Section
import net.tisma.app.ui.TismaApp
import net.tisma.app.ui.TismaTheme
import net.tisma.mockprinters.MockBambu
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.shadows.ShadowLooper
import java.io.File
import kotlin.test.assertEquals

/** gcode_macros.md, rule 12: the send options chosen on the plate reach a Bambu Lab's print command. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35], qualifiers = "w411dp-h891dp-xxhdpi")
class BambuSendTest {
    @get:Rule
    val compose = createComposeRule()

    @get:Rule
    val tmp = TemporaryFolder()

    private fun wait(tag: String) = compose.waitUntil(10_000) {
        ShadowLooper.idleMainLooper()
        compose.onAllNodes(hasTestTag(tag)).fetchSemanticsNodes().isNotEmpty()
    }

    @Test
    fun `turning levelling off and timelapse on goes in the project_file`() {
        val bambu = MockBambu().start()
        try {
            val context: Context = ApplicationProvider.getApplicationContext()
            val settings = AppSettings(context).apply { packWizardDone = true }
            val work = tmp.newFolder("work")
            val exe = File(work, "engine.py").apply {
                writeBytes(BambuSendTest::class.java.getResourceAsStream("/fake_library_engine.py")!!.readBytes()); setExecutable(true)
            }
            val slice = SliceViewModel(EngineRunner(exe, work), work, saved = settings)
            val store = PrinterStore(context, MemorySecrets())
            store.save(net.tisma.printers.PrinterConfig(PrinterStore.newId(), "A1", net.tisma.printers.PrinterKind.BAMBU, bambu.url, ""),
                bambu.accessCode)
            val printers = PrintersViewModel(store, pollMs = 50, connector = { c, secret ->
                net.tisma.printers.BambuConnection.of(c.url, secret.orEmpty(), null) })
            val cloud = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5)
            compose.setContent { TismaTheme { TismaApp(cloud, printers, slice, settings, Navigator(Section.PLATE)) } }
            compose.runOnIdle { slice.open("cube.stl") { File("../../../tests/data/cube_ascii.stl").inputStream() } }
            compose.waitUntil(10_000) { ShadowLooper.idleMainLooper(); slice.state.value.modelName != null }
            slice.slice(checkMemory = false)
            wait("send_to_A1")
            compose.onNodeWithTag("send_to_A1").performScrollTo().performClick()
            // Levelling and flow calibration start on; turn levelling off and the timelapse on.
            compose.onNodeWithTag("bambu_option_leveling").performScrollTo().performClick()
            compose.onNodeWithTag("bambu_option_timelapse").performScrollTo().performClick()
            compose.onNodeWithTag("plate_send").performScrollTo().performClick()
            compose.waitUntil(15_000) {
                ShadowLooper.idleMainLooper()
                bambu.commands.any { it["command"].toString() == "\"project_file\"" }
            }
            val pf = bambu.commands.last { it["command"].toString() == "\"project_file\"" }
            assertEquals("false", pf["bed_levelling"].toString())
            assertEquals("true", pf["flow_cali"].toString())
            assertEquals("true", pf["timelapse"].toString())
        } finally {
            bambu.stop()
        }
    }
}
