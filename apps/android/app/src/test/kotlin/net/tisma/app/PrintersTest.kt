package net.tisma.app

import android.content.Context
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.hasText
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performTextInput
import androidx.compose.ui.test.printToString
import androidx.test.core.app.ApplicationProvider
import net.tisma.app.cloud.CloudViewModel
import net.tisma.app.data.AppSettings
import net.tisma.app.data.KeystoreSecretStore
import net.tisma.app.printers.PrinterStore
import net.tisma.app.printers.PrintersViewModel
import net.tisma.app.ui.Navigator
import net.tisma.app.ui.Section
import net.tisma.app.ui.TismaApp
import net.tisma.app.ui.TismaTheme
import net.tisma.mockprinters.MockMoonraker
import net.tisma.mockprinters.MockOctoPrint
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.io.File
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** The Printer section against simulated printers (MA7, MA8, N1). */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35], qualifiers = "w411dp-h891dp-xxhdpi")
class PrintersTest {
    @get:Rule
    val compose = createComposeRule()

    private val context: Context = ApplicationProvider.getApplicationContext()
    private val moonraker = MockMoonraker().apply { start() }
    private val octoprint = MockOctoPrint().apply { start() }
    private lateinit var printers: PrintersViewModel
    private lateinit var store: PrinterStore
    private val nav = Navigator()

    @Before
    fun start() {
        val settings = AppSettings(context)
        val secrets = KeystoreSecretStore(context) { testKey }
        store = PrinterStore(context, secrets)
        printers = PrintersViewModel(store, pollMs = 20)
        val cloud = CloudViewModel(settings, secrets, "Test", pollMs = 5)
        compose.setContent { TismaTheme { TismaApp(cloud, printers, noEngine(context), settings, nav) } }
        compose.runOnIdle { nav.select(Section.PRINTER) }
    }

    @After
    fun stop() {
        moonraker.stop()
        octoprint.stop()
    }

    private fun waitState(condition: () -> Boolean) = compose.waitUntil(10_000) {
        org.robolectric.shadows.ShadowLooper.idleMainLooper()
        condition()
    }

    private fun dump() {
        println("PRINTERS " + printers.state.value)
        println(compose.onAllNodes(androidx.compose.ui.test.isRoot(), useUnmergedTree = true).printToString())
    }

    private fun waitFor(tag: String) = try { compose.waitUntil(10_000) {
        org.robolectric.shadows.ShadowLooper.idleMainLooper()
        compose.onAllNodes(hasTestTag(tag), useUnmergedTree = true).fetchSemanticsNodes().isNotEmpty()
    } } catch (e: Throwable) { dump(); throw e }

    private fun waitForText(text: String) = try { compose.waitUntil(10_000) {
        org.robolectric.shadows.ShadowLooper.idleMainLooper()
        compose.onAllNodes(hasText(text, substring = true), useUnmergedTree = true).fetchSemanticsNodes().isNotEmpty()
    } } catch (e: Throwable) { dump(); throw e }

    private fun addPrinter(kind: String, address: String, key: String?) {
        compose.onNodeWithTag("add_printer").performClick()
        waitFor("printer_address")
        compose.onNodeWithTag("kind_$kind").performClick()
        compose.onNodeWithTag("printer_name").performTextInput("Test printer")
        compose.onNodeWithTag("printer_address").performTextInput(address)
        key?.let { compose.onNodeWithTag("printer_key").performTextInput(it) }
        compose.onNodeWithTag("printer_save").performClick()
        waitState { printers.state.value.printers.isNotEmpty() }
    }

    @Test
    fun `MA7 a G-code sent to a Klipper printer starts, shows its progress, and pauses`() {
        // Slow enough that the print never ends while the test pauses and cancels it.
        moonraker.stepPercent = 0.01
        addPrinter("moonraker", moonraker.url, null)
        val id = printers.state.value.printers.single().id
        waitFor("printer_$id")
        compose.runOnIdle { nav.open(net.tisma.app.ui.Screen.PrinterDetail(id)) }
        val gcode = "G28\nG1 X10 Y10 E1\n".toByteArray()
        compose.runOnIdle { printers.upload(id, "cube.gcode", gcode.size.toLong(), true) { gcode.inputStream() } }
        waitState { moonraker.state == "printing" || moonraker.state == "complete" }
        assertTrue(moonraker.files.getValue("cube.gcode").contentEquals(gcode))
        waitFor("printer_pause")
        waitForText("Nozzle 210 / 210 °C")
        compose.onNodeWithTag("printer_pause").performClick()
        waitState { moonraker.state == "paused" }
        waitFor("printer_resume")
        compose.onNodeWithTag("printer_cancel").performClick()
        waitFor("printer_cancel_confirm")
        compose.onNodeWithTag("printer_cancel_confirm").performClick()
        waitState { moonraker.state == "cancelled" }
    }

    @Test
    fun `CM1 the printer's camera shows on its page`() {
        addPrinter("moonraker", moonraker.url, null)
        val id = printers.state.value.printers.single().id
        compose.runOnIdle { nav.open(net.tisma.app.ui.Screen.PrinterDetail(id)) }
        waitState { printers.state.value.cameras[id] is net.tisma.app.printers.CameraView.Image }
        waitFor("camera_image")
    }

    @Test
    fun `BD2 a printer found on the network is offered once and added with its address`() {
        val f = net.tisma.printers.FoundPrinter(net.tisma.printers.PrinterKind.OCTOPRINT, "octopi",
            "127.0.0.1", octoprint.url.substringAfterLast(':').toInt(), "/")
        compose.runOnIdle { printers.onFound(f); printers.onFound(f) }
        waitFor("found_" + f.key)
        compose.onNodeWithTag("found_" + f.key).performClick()
        waitFor("printer_key")
        compose.onNodeWithTag("printer_key").performTextInput("test-key")
        compose.onNodeWithTag("printer_save").performClick()
        waitState { printers.state.value.printers.size == 1 }
        assertEquals(octoprint.url, printers.state.value.printers.single().url)
        // Saved, it is no longer offered.
        waitState { printers.state.value.found.isEmpty() }
    }

    @Test
    fun `MA8 the OctoPrint key is kept encrypted and a wrong one is refused`() {
        compose.onNodeWithTag("add_printer").performClick()
        waitFor("printer_address")
        compose.onNodeWithTag("kind_octoprint").performClick()
        compose.onNodeWithTag("printer_address").performTextInput(octoprint.url)
        compose.onNodeWithTag("printer_key").performTextInput("wrong-key")
        compose.onNodeWithTag("printer_save").performClick()
        // The test failed: nothing is saved, the reason is told (the notice
        // goes by too fast to read in a test: the session log has it), and
        // the key is not in the log.
        waitState { net.tisma.app.data.AppLog.text().contains("printer: auth Invalid API key") }
        assertTrue(printers.state.value.printers.isEmpty())
        assertFalse(net.tisma.app.data.AppLog.text().contains("wrong-key"))
    }

    @Test
    fun `an OctoPrint printer with its key is saved, and the key never appears as text`() {
        addPrinter("octoprint", octoprint.url, "test-key")
        val p = printers.state.value.printers.single()
        assertEquals("test-key", store.secret(p.id))
        waitState { printers.state.value.status[p.id] != null }
        assertEquals(net.tisma.printers.PrinterState.READY, printers.state.value.status.getValue(p.id).state)
        val data = File(context.applicationInfo.dataDir)
        waitState { File(data, "shared_prefs/${PrinterStore.FILE}.xml").exists() }
        assertFalse(data.walkTopDown().filter { it.isFile }.any { it.readText(Charsets.ISO_8859_1).contains("test-key") })
    }
}
