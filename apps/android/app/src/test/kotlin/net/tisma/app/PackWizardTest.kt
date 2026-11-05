package net.tisma.app

import android.content.Context
import androidx.compose.ui.test.assertTextEquals
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performSemanticsAction
import androidx.compose.ui.test.performTextInput
import androidx.test.core.app.ApplicationProvider
import net.tisma.app.cloud.CloudViewModel
import net.tisma.app.data.AppSettings
import net.tisma.app.data.KeystoreSecretStore
import net.tisma.app.engine.EngineRunner
import net.tisma.app.engine.ProfilePack
import net.tisma.app.engine.SettingsLayout
import net.tisma.app.engine.SliceViewModel
import net.tisma.app.printers.PrinterStore
import net.tisma.app.printers.PrintersViewModel
import net.tisma.app.ui.Navigator
import net.tisma.app.ui.Screen
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

/** MA1: the first start opens the profile pack's wizard; a printer chosen there goes to the printer card. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35], qualifiers = "w411dp-h891dp-xxhdpi")
class PackWizardTest {
    @get:Rule
    val compose = createComposeRule()

    @get:Rule
    val tmp = TemporaryFolder()

    private val index = """{"pack_version":"2026.11.04","vendors":[{"id":"Acme","name":"Acme 3D","models":[
        {"name":"Acme One","file":"vendors/Acme/models/One.json","technology":"fff",
         "variants":[{"id":"0.4","nozzle":0.4},{"id":"0.6","nozzle":0.6}],"default_variant":"0.4"},
        {"name":"Acme Resin","file":"vendors/Acme/models/R.json","technology":"sla","variants":[{"id":"r","nozzle":0}]}]},
        {"id":"Lib","name":"Filament library","models":[],"filaments":2}]}"""

    private fun engine(base: String): File = tmp.newFile("engine.sh").apply {
        writeText("""#!/bin/sh
            |first="${'$'}1"; cmd="${'$'}2"; data=""; from=""; index=""; vendors=""; out=""
            |while [ ${'$'}# -gt 0 ]; do case "${'$'}1" in --data) data="${'$'}2"; shift;; --from) from="${'$'}2"; shift;; --index) index="${'$'}2"; shift;; --vendors) vendors="${'$'}2"; shift;; -o) out="${'$'}2"; shift;; esac; shift; done
            |if [ "${'$'}first" = "profile" ]; then echo '{"speed":{"perimeter":50,"travel":180}}' > "${'$'}out"
            |elif [ "${'$'}cmd" = "plan" ]; then
            |  if [ -z "${'$'}index" ]; then echo '{"downloads":[{"url":"$base/index.json","name":"index.json","size":0,"sha256":""}]}'
            |  else echo "${'$'}vendors" > "${'$'}data/installed_vendors"; echo '{"downloads":[{"url":"$base/Acme.tar.xz","name":"Acme.tar.xz","size":4,"sha256":"x"}],"published":"2026.11.04"}'; fi
            |elif [ "${'$'}cmd" = "install" ]; then
            |  mkdir -p "${'$'}data/pack"; cp -r "${'$'}from"/. "${'$'}data/pack"/
            |  p="${'$'}data/pack/vendors"; mkdir -p "${'$'}p/Acme/models" "${'$'}p/Acme/filaments" "${'$'}p/Acme/prints" "${'$'}p/Lib/filaments"
            |  echo '{"license":"CC0-1.0","origin":"Acme profiles","variants":[{"id":"0.6","defaults":{"filament":["vendors/Acme/filaments/Acme_PLA.json"],"print":["vendors/Acme/prints/Normal.json"]}}]}' > "${'$'}p/Acme/models/One.json"
            |  echo '{"compatible":null}' > "${'$'}p/Acme/filaments/Acme_PLA.json"
            |  echo '{"generic":"true"}' > "${'$'}p/Lib/filaments/Generic_PETG.json"
            |  echo '{}' > "${'$'}p/Acme/prints/Normal.json"; echo '{}' > "${'$'}p/Acme/prints/Fast.json"
            |  echo '{"pack_version":"2026.11.04"}'
            |elif [ "${'$'}cmd" = "list" ]; then echo '{"vendors":[]}'
            |else exit 1; fi
            |""".trimMargin())
        setExecutable(true)
    }

    @Test
    fun `MA1 the wizard lists the pack's printers and puts the chosen one on the printer card`() {
        val server = net.tisma.mockcloud.StaticFiles(mapOf("index.json" to index.toByteArray(), "Acme.tar.xz" to "ACME".toByteArray()))
        try {
            val context: Context = ApplicationProvider.getApplicationContext()
            val settings = AppSettings(context)
            val exe = engine(server.url)
            val work = tmp.newFolder("work")
            val data = tmp.newFolder("profiles")
            val slice = SliceViewModel(EngineRunner(exe, work, data), work, saved = settings,
                pack = ProfilePack(exe, data), packUrl = server.url)
            val cloud = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5)
            val printers = PrintersViewModel(PrinterStore(context, MemorySecrets()))
            val nav = Navigator()
            compose.setContent { TismaTheme { TismaApp(cloud, printers, slice, settings, nav) } }
            wait("pack_wizard")
            wait("pack_model_Acme One")
            // Resin printers are not offered on the phone.
            compose.runOnIdle {
                assertTrue(slice.state.value.packIndex!!.vendors.single().models.none { it.name == "Acme Resin" })
            }
            compose.onNodeWithTag("pack_search").performTextInput("one")
            compose.onNodeWithTag("pack_model_Acme One").performClick()
            wait("pack_variant_0.6")
            compose.onNodeWithTag("pack_variant_0.6").performClick()
            compose.onNodeWithTag("pack_use").performClick()
            compose.waitUntil(10_000) {
                org.robolectric.shadows.ShadowLooper.idleMainLooper()
                slice.state.value.profile != null && nav.stack.none { it is Screen.PackWizard }
            }
            // Rule 13: the variant's default filament; the filament library comes with the printer.
            assertEquals("vendors/Acme/filaments/Acme_PLA.json", slice.state.value.profile!!.filament)
            assertEquals("vendors/Acme/prints/Normal.json", slice.state.value.profile!!.print)
            assertEquals("Acme,Lib", File(data, "installed_vendors").readText().trim())
            // Then the filament step, with the pack's filaments for that printer.
            wait("pack_filaments")
            wait("pack_filament_Generic PETG")
            compose.onNodeWithTag("pack_filament_Acme PLA").assertExists()
            // Rule 15: the printer's licence and origin.
            wait("pack_license")
            compose.onNodeWithTag("pack_license").assertTextEquals("CC0-1.0 · Acme profiles")
            // BH27: another profile replaces the printer settings, except those changed by hand, which are said.
            compose.runOnIdle { slice.setValue("speed.perimeter", kotlinx.serialization.json.JsonPrimitive(30)) }
            compose.onNodeWithTag("pack_filament_Generic PETG").performClick()
            compose.waitUntil(10_000) {
                org.robolectric.shadows.ShadowLooper.idleMainLooper()
                slice.state.value.profile?.filament == "vendors/Lib/filaments/Generic_PETG.json"
            }
            val kept = context.getString(net.tisma.app.R.string.profile_kept_changes, "").trim()
            compose.waitUntil(10_000) {
                org.robolectric.shadows.ShadowLooper.idleMainLooper()
                compose.onAllNodes(androidx.compose.ui.test.hasText(kept, substring = true)).fetchSemanticsNodes().isNotEmpty()
            }
            compose.runOnIdle {
                val s = slice.state.value
                assertEquals("30", SettingsLayout.show(s.value("speed.perimeter")))
                assertEquals("180", SettingsLayout.show(s.value("speed.travel")))
            }
            wait("pack_print_Fast")
            compose.onNodeWithTag("pack_print_Fast").performScrollTo().performClick()
            compose.waitUntil(10_000) {
                org.robolectric.shadows.ShadowLooper.idleMainLooper()
                slice.state.value.profile?.print == "vendors/Acme/prints/Fast.json"
            }
            // The notice's snackbar may lie over the button: its action, not a touch.
            compose.onNodeWithTag("pack_done").performSemanticsAction(androidx.compose.ui.semantics.SemanticsActions.OnClick)
            val p = slice.state.value.profile!!
            assertEquals("vendors/Acme/models/One.json#0.6", p.printer)
            assertEquals(0.6, p.nozzle)
            assertTrue(p.pack)
            assertEquals("Acme One", p.name)
            assertTrue(settings.packWizardDone)
            assertTrue(File(data, "pack/Acme.tar.xz").isFile)
            wait("printer_card")
        } finally {
            server.close()
        }
    }

    @Test
    fun `rules 19 and 20 a newer pack is offered once a day, and a chosen printer that left the pack is said`() {
        val server = net.tisma.mockcloud.StaticFiles(mapOf("index.json" to index.toByteArray(), "Acme.tar.xz" to "ACME".toByteArray()))
        try {
            val context: Context = ApplicationProvider.getApplicationContext()
            val settings = AppSettings(context).apply {
                packWizardDone = true
                profilePrinter = "vendors/Acme/models/Gone.json#0.4"; profilePack = true; profileName = "Acme Gone"
            }
            val exe = engine(server.url)
            val work = tmp.newFolder("work")
            val data = tmp.newFolder("profiles")
            File(data, "pack").mkdirs()
            File(data, "pack/index.json").writeText("""{"pack_version":"2026.10.01","vendors":[]}""")
            val slice = SliceViewModel(EngineRunner(exe, work, data), work, saved = settings, pack = ProfilePack(exe, data), packUrl = server.url)
            val cloud = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5)
            val printers = PrintersViewModel(PrinterStore(context, MemorySecrets()))
            val nav = Navigator()
            compose.setContent { TismaTheme { TismaApp(cloud, printers, slice, settings, nav) } }
            wait("pack_update")
            assertEquals("2026.11.04", slice.state.value.packNewer)
            assertTrue(settings.packLastCheck > 0)
            compose.onNodeWithTag("pack_update").performClick()
            compose.waitUntil(10_000) {
                org.robolectric.shadows.ShadowLooper.idleMainLooper()
                nav.notices.any { it.contains("no longer in the pack") }
            }
            // It stays chosen (rule 20), and the line is gone.
            assertEquals("Acme Gone", slice.state.value.profile!!.name)
            assertEquals(null, slice.state.value.packNewer)
            // The same day, no new look.
            slice.checkPackOnOpen()
            assertEquals(null, slice.state.value.packNewer)
        } finally {
            server.close()
        }
    }

    private fun wait(tag: String) = compose.waitUntil(10_000) {
        org.robolectric.shadows.ShadowLooper.idleMainLooper()
        compose.onAllNodes(hasTestTag(tag), useUnmergedTree = true).fetchSemanticsNodes().isNotEmpty()
    }
}
