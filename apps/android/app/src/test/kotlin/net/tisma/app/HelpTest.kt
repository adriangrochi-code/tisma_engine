package net.tisma.app

import android.content.Context
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performTextInput
import androidx.test.core.app.ApplicationProvider
import kotlinx.serialization.json.JsonPrimitive
import net.tisma.app.cloud.CloudViewModel
import net.tisma.app.data.AppSettings
import net.tisma.app.data.KeystoreSecretStore
import net.tisma.app.engine.EngineRunner
import net.tisma.app.engine.Knowledge
import net.tisma.app.engine.ProfilePack
import net.tisma.app.engine.SliceViewModel
import net.tisma.app.printers.PrinterStore
import net.tisma.app.printers.PrintersViewModel
import net.tisma.app.ui.Navigator
import net.tisma.app.ui.Screen
import net.tisma.app.ui.Section
import net.tisma.app.ui.TismaApp
import net.tisma.app.ui.TismaTheme
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.io.File
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** The offline help from the pack's knowledge (mobile_app.md, rule 16), with made-up entries in the pack's format. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35], qualifiers = "w411dp-h891dp-xxhdpi")
class HelpTest {
    @get:Rule
    val compose = createComposeRule()

    @get:Rule
    val tmp = TemporaryFolder()

    private fun pack(): File {
        val data = tmp.newFolder("profiles")
        val k = File(data, "pack/knowledge").apply { mkdirs() }
        File(k, "problems.json").writeText("""{"format":"tisma-knowledge-problems","version":1,"language":"en",
            "sources":[{"id":"s","title":"Test guide","author":"Someone"}],
            "problems":[{"id":"stringing","name":"Thin strings between parts","symptoms":"Hairs across gaps.",
              "causes":[{"name":"Too little retraction","explain":"The nozzle leaks while it travels.","check":"Strings on every travel?",
                "actions":[{"type":"setting","key":"retraction.length","op":"set","value":1.2,"why":"Pull back more."},
                           {"type":"procedure","id":"dry"}]}]}]}""")
        File(k, "procedures.json").writeText("""{"format":"tisma-knowledge-procedures","version":1,
            "procedures":[{"id":"dry","name":"Dry the filament","for":"Wet filament","steps":[{"title":"Heat","text":"Four hours at 50 °C."}]}]}""")
        File(k, "glossary.json").writeText("""{"format":"tisma-knowledge-glossary","version":1,"terms":[{"term":"Retraction","definition":"Pulling the filament back."}]}""")
        return data
    }

    @Test
    fun `the knowledge is read leniently`() {
        val k = Knowledge.read(File(pack(), "pack/knowledge"))
        assertEquals(1, k.problems.size)
        assertEquals("retraction.length", k.problems[0].causes[0].actions[0].key)
        assertTrue(k.materials.isEmpty() && k.calibrations.isEmpty())
        assertEquals(listOf("Test guide, Someone"), k.sources["problems"])
        assertTrue(Knowledge.read(File(tmp.root, "none")).empty)
    }

    @Test
    fun `rule 16 a problem's cause applies its setting and opens its procedure`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val settings = AppSettings(context).apply { packWizardDone = true }
        val work = tmp.newFolder("work")
        val data = pack()
        val exe = File(work, "fake").apply { writeText("#!/bin/sh\nexit 1\n"); setExecutable(true) }
        val slice = SliceViewModel(EngineRunner(exe, work, data), work, saved = settings, pack = ProfilePack(exe, data))
        val cloud = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5)
        val printers = PrintersViewModel(PrinterStore(context, MemorySecrets()))
        val nav = Navigator()
        compose.setContent { TismaTheme { TismaApp(cloud, printers, slice, settings, nav) } }
        compose.runOnIdle { nav.select(Section.MORE); nav.open(Screen.Help) }
        wait("help_problems/stringing")
        compose.onNodeWithTag("help_problems/stringing").performClick()
        wait("help_apply_retraction.length")
        compose.onNodeWithTag("help_apply_retraction.length").performClick()
        compose.runOnIdle { assertEquals(JsonPrimitive(1.2), slice.state.value.settings.changes["retraction.length"]) }
        compose.onNodeWithText("Dry the filament").performClick()
        wait("help_procedures/dry")
        compose.onNodeWithText("Four hours at 50 °C.").assertExists()
        // The search goes through every kind.
        compose.onNodeWithTag("help_search").performTextInput("pulling")
        wait("help_glossary/Retraction")
    }

    private fun wait(tag: String) = compose.waitUntil(10_000) {
        org.robolectric.shadows.ShadowLooper.idleMainLooper()
        compose.onAllNodes(hasTestTag(tag), useUnmergedTree = true).fetchSemanticsNodes().isNotEmpty()
    }
}
