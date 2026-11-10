package net.tisma.app

import android.content.Context
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.test.core.app.ApplicationProvider
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
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
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.shadows.ShadowLooper
import java.io.File
import kotlin.test.assertEquals
import kotlin.test.assertFalse

/** gcode_macros.md, rule 12: a Bambu Lab's plate is a slicing setting (its start G-code's curr_bed_type). */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35], qualifiers = "w411dp-h891dp-xxhdpi")
class BambuPlateTest {
    @get:Rule
    val compose = createComposeRule()

    @get:Rule
    val tmp = TemporaryFolder()

    private fun wait(tag: String) = compose.waitUntil(10_000) {
        ShadowLooper.idleMainLooper()
        compose.onAllNodes(hasTestTag(tag)).fetchSemanticsNodes().isNotEmpty()
    }

    @Test
    fun `a Bambu Lab printer offers its plates, and the chosen one goes into the slice settings`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val settings = AppSettings(context).apply { packWizardDone = true }
        val work = tmp.newFolder("work")
        val exe = File(work, "engine").apply { writeText("#!/bin/sh\nexit 1\n"); setExecutable(true) }
        val runner = EngineRunner(exe, work)
        val slice = SliceViewModel(runner, work, saved = settings)
        val cloud = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5)
        val printers = PrintersViewModel(PrinterStore(context, MemorySecrets()))
        compose.setContent { TismaTheme { TismaApp(cloud, printers, slice, settings, Navigator(Section.PLATE)) } }
        compose.runOnIdle {
            slice.setProfile(EngineRunner.ProfileChoice("vendors/Orca_BBL/models/Bambu_Lab_A1_mini.json#0.4", 0.4, null, pack = true,
                name = "Bambu Lab A1 mini"))
            slice.open("cube.stl") { File("../../../tests/data/cube_ascii.stl").inputStream() }
        }
        wait("bambu_plate_auto")
        compose.onNodeWithTag("bambu_plate_Textured PEI Plate").performScrollTo().performClick()
        compose.runOnIdle {
            val s = slice.state.value.settings
            val config = Json.parseToJsonElement(runner.writeConfig(s, null).readText()) as JsonObject
            assertEquals("Textured PEI Plate", config["bambu"]!!.jsonObject["plate"]!!.jsonPrimitive.content)
        }
        // «Auto» takes it out: the printer detects its plate.
        compose.onNodeWithTag("bambu_plate_auto").performScrollTo().performClick()
        compose.runOnIdle { assertFalse("bambu.plate" in slice.state.value.settings.changes) }
    }
}
