package net.tisma.app

import android.content.Context
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performScrollToNode
import androidx.compose.ui.test.performTextReplacement
import androidx.test.core.app.ApplicationProvider
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import net.tisma.app.cloud.CloudViewModel
import net.tisma.app.data.AppSettings
import net.tisma.app.data.KeystoreSecretStore
import net.tisma.app.engine.EngineRunner
import net.tisma.app.engine.PanelVisibility
import net.tisma.app.engine.PanelVisibility.View
import net.tisma.app.engine.SettingsLayout
import net.tisma.app.engine.SettingsLayout.Editor
import net.tisma.app.engine.SliceViewModel
import net.tisma.app.printers.PrinterStore
import net.tisma.app.printers.PrintersViewModel
import net.tisma.app.ui.Navigator
import net.tisma.app.ui.Section
import net.tisma.app.ui.TismaApp
import net.tisma.app.ui.TismaTheme
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.io.File
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** Print and Material from the desktop's panel (mobile_app.md, rule 7; settings_panel.md). */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35], qualifiers = "w411dp-h891dp-xxhdpi")
class SettingsPanelTest {
    @get:Rule
    val compose = createComposeRule()

    private fun layout(lang: String = "en") = SettingsLayout.parse(File("src/main/assets/settings/layout_$lang.json").readText())

    @Test
    fun `MA12 the Basic level shows the keys of settings_panel md rule 10`() {
        val l = layout()
        fun basic(tab: String) = l.tab(tab)!!.sections.flatMap { s -> s.subsections.flatMap { it.rows } }
            .filter { PanelVisibility.visible(it, "", View.BASIC, emptySet(), false) { k -> if (k == "support.enabled") JsonPrimitive(true) else null } }
            .map { it.key }.toSet()
        assertEquals(setOf("layers.height", "layers.first_height", "walls.seam", "walls.count", "surfaces.top_layers",
            "surfaces.bottom_layers", "surfaces.top_min_thickness", "surfaces.bottom_min_thickness", "surfaces.top_pattern",
            "infill.density", "infill.pattern", "support.enabled", "support.automatic", "adhesion.skirt_loops",
            "adhesion.skirt_distance", "adhesion.brim_width"), basic("print"))
        // The engine keeps material.nozzle_temp and bed_temp as machine.variables (docs/CONFIG.md); the first layer's
        // temperatures of rule 10 are not in its panel yet (asked to the desktop session).
        assertEquals(setOf("machine.variables.temperature", "machine.variables.bed_temperature",
            "cooling.fan_min", "cooling.fan_max", "retraction.length", "retraction.lift"), basic("material"))
        // Both languages carry the same rows.
        assertEquals(l.rows.keys, layout("es").rows.keys)
    }

    @Test
    fun `rows are edited by their type and range`() {
        val r = layout().rows
        assertEquals(Editor.Number(false, 0.01, 10.0), r["layers.height"]!!.editor)
        assertEquals(Editor.Number(true, 0.0, 1000.0), r["walls.count"]!!.editor)
        assertEquals(Editor.Toggle, r["support.enabled"]!!.editor)
        assertEquals(Editor.NumberOrPercent, r["layers.first_line_width"]!!.editor)
        assertTrue((r["infill.pattern"]!!.editor as Editor.Choice).options.containsAll(listOf("gyroid", "rectilinear")))
        // "the same as top_pattern"
        assertEquals(r["surfaces.top_pattern"]!!.editor, r["surfaces.bottom_pattern"]!!.editor)
        assertEquals(Editor.ReadOnly, r["layers.heights"]!!.editor)
        // No documented type: a number by its value.
        assertEquals(Editor.Number(false, 0.0, 600.0), r["machine.variables.temperature"]!!.editor)
        assertEquals(JsonPrimitive(0.15), SettingsLayout.toJson(r["layers.height"]!!.editor, "0,15"))
        assertNull(SettingsLayout.toJson(r["layers.height"]!!.editor, "11"))
        assertNull(SettingsLayout.toJson(r["walls.count"]!!.editor, "2.5"))
        assertEquals(JsonPrimitive("150%"), SettingsLayout.toJson(Editor.NumberOrPercent, "150 %"))
        assertNull(SettingsLayout.toJson(r["infill.pattern"]!!.editor, "spiral"))
    }

    @Test
    fun `changes go to the engine as nested settings, and flatten back`() {
        val s = EngineRunner.SimpleSettings(0.16, true, mapOf("infill.density" to JsonPrimitive(25), "infill.pattern" to JsonPrimitive("gyroid"),
            "walls.count" to JsonPrimitive(4)))
        val j = s.toJson()
        assertEquals("gyroid", j["infill"]!!.jsonObject["pattern"]!!.jsonPrimitive.content)
        assertEquals("0.16", j["layers"]!!.jsonObject["height"]!!.jsonPrimitive.content)
        assertEquals("true", j["support"]!!.jsonObject["enabled"]!!.jsonPrimitive.content)
        val flat = SettingsLayout.flatten(j)
        assertEquals(JsonPrimitive(4), flat["walls.count"])
    }

    @Test
    fun `rule 6 a switch that is off hides what depends on it`() {
        fun v(on: Boolean) = { k: String -> if (k == "support.enabled" || k == "zaa.enabled") JsonPrimitive(on) else null }
        assertTrue(PanelVisibility.hiddenByParent("support.style", "support", v(false)))
        assertFalse(PanelVisibility.hiddenByParent("support.style", "support", v(true)))
        assertFalse(PanelVisibility.hiddenByParent("support.enabled", "support", v(false)))
        assertTrue(PanelVisibility.hiddenByParent("zaa.raise", "quality", v(false)))
    }

    @Test
    fun `the Print screen edits a setting, keeps it and puts it back`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val settings = AppSettings(context)
        val work = File(context.cacheDir, "engine")
        val slice = SliceViewModel(EngineRunner(File(work, "none"), work), work, saved = settings, loadLayout = { layout() })
        val cloud = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5)
        val nav = Navigator()
        val printers = PrintersViewModel(PrinterStore(context, MemorySecrets()))
        compose.setContent { TismaTheme { TismaApp(cloud, printers, slice, settings, nav) } }
        compose.runOnIdle { nav.select(Section.PRINT) }
        wait("panel_section_strength")
        // Normal shows more than Basic.
        compose.onNodeWithTag("panel_view_normal").performClick()
        compose.runOnIdle { assertEquals(View.NORMAL, slice.state.value.view) }
        compose.onNodeWithTag("panel_print").performScrollToNode(hasTestTag("panel_section_strength"))
        compose.onNodeWithTag("panel_section_strength").performClick()
        wait("row_walls.count")
        compose.onNodeWithTag("panel_section").performScrollToNode(hasTestTag("edit_walls.count"))
        compose.onNodeWithTag("edit_walls.count").performTextReplacement("4")
        compose.runOnIdle { assertEquals(JsonPrimitive(4L), slice.state.value.settings.changes["walls.count"]) }
        assertTrue(settings.panelChanges!!.contains("\"walls.count\":4"))
        // Out of range: not set.
        compose.onNodeWithTag("edit_walls.count").performTextReplacement("-1")
        compose.runOnIdle { assertEquals(JsonPrimitive(4L), slice.state.value.settings.changes["walls.count"]) }
        compose.onNodeWithTag("reset_walls.count").performClick()
        compose.runOnIdle { assertNull(slice.state.value.settings.changes["walls.count"]) }
        // A choice.
        compose.onNodeWithTag("panel_section").performScrollToNode(hasTestTag("edit_infill.pattern"))
        compose.onNodeWithTag("edit_infill.pattern").performClick()
        compose.onNodeWithTag("option_infill.pattern_gyroid").performClick()
        compose.runOnIdle { assertEquals(JsonPrimitive("gyroid"), slice.state.value.settings.changes["infill.pattern"]) }
        // The layer height row is the plate's chip.
        compose.runOnIdle { nav.back() }
        wait("panel_changed_strength")
        compose.onNodeWithTag("panel_print").performScrollToNode(hasTestTag("panel_section_quality"))
        compose.onNodeWithTag("panel_section_quality").performClick()
        wait("edit_layers.height")
        compose.onNodeWithTag("edit_layers.height").performTextReplacement("0.12")
        compose.runOnIdle { assertEquals(0.12, slice.state.value.settings.layerHeight) }
    }

    @Test
    fun `rule 8 the printer's configuration opens from the Printer section`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val settings = AppSettings(context)
        val work = File(context.cacheDir, "engine")
        val slice = SliceViewModel(EngineRunner(File(work, "none"), work), work, saved = settings, loadLayout = { layout() })
        val cloud = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5)
        val printers = PrintersViewModel(PrinterStore(context, MemorySecrets()))
        val nav = Navigator()
        compose.setContent { TismaTheme { TismaApp(cloud, printers, slice, settings, nav) } }
        compose.runOnIdle { nav.select(Section.PRINTER) }
        wait("printer_settings")
        compose.onNodeWithTag("printer_settings").performClick()
        wait("panel_section_machine")
        compose.onNodeWithTag("panel_view_advanced").performClick()
        compose.onNodeWithTag("panel_printer").performScrollToNode(hasTestTag("printer_notes"))
        compose.onNodeWithTag("printer_notes").performTextReplacement("Glass bed")
        compose.runOnIdle { assertEquals("Glass bed", settings.printerNotes(null)) }
    }

    @Test
    @Config(qualifiers = "w900dp-h600dp-land-xhdpi")
    fun `on a tablet the sections and the chosen one are side by side, as the plate's view and controls`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val settings = AppSettings(context)
        val work = File(context.cacheDir, "engine")
        val slice = SliceViewModel(EngineRunner(File(work, "none"), work), work, saved = settings, loadLayout = { layout() })
        val cloud = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5)
        val printers = PrintersViewModel(PrinterStore(context, MemorySecrets()))
        val nav = Navigator()
        compose.setContent { TismaTheme { TismaApp(cloud, printers, slice, settings, nav) } }
        compose.runOnIdle { nav.select(Section.PRINT) }
        wait("panel_section_strength")
        wait("panel_sub_layer_height")
        compose.onNodeWithTag("panel_section_strength").performClick()
        wait("row_walls.count")
        compose.runOnIdle { assertTrue(nav.stack.isEmpty()) }
        // The plate: the 3D view and the controls together once a model is open.
        compose.runOnIdle { nav.select(Section.PLATE); slice.open("cube.stl") { File("../../../tests/data/cube_ascii.stl").inputStream() } }
        wait("plate_3d")
        wait("printer_card")
    }

    private fun wait(tag: String) = compose.waitUntil(10_000) {
        org.robolectric.shadows.ShadowLooper.idleMainLooper()
        compose.onAllNodes(hasTestTag(tag), useUnmergedTree = true).fetchSemanticsNodes().isNotEmpty()
    }
}
