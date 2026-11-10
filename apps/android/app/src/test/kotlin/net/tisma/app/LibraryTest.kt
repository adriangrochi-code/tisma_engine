package net.tisma.app

import android.content.Context
import androidx.compose.ui.semantics.getOrNull
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performTextInput
import androidx.test.core.app.ApplicationProvider
import net.tisma.app.cloud.CloudViewModel
import net.tisma.app.data.AppSettings
import net.tisma.app.data.KeystoreSecretStore
import net.tisma.app.engine.EngineRunner
import net.tisma.app.engine.Library
import net.tisma.app.engine.LibraryViewModel
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
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertIs
import kotlin.test.assertTrue

/**
 * «Mi biblioteca» (local_library.md) against a stand-in engine that answers
 * the `library` commands as agreed with the engine session
 * (fake_library_engine.py): ML1, ML2, ML3/ML4, ML6, ML10 and an export
 * imported back (ML5's phone half).
 */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35], qualifiers = "w411dp-h891dp-xxhdpi")
class LibraryTest {
    @get:Rule
    val compose = createComposeRule()

    @get:Rule
    val tmp = TemporaryFolder()

    /** The real engine when TISMA_HOST_ENGINE names a host build (its `library` commands), else the stand-in. */
    private fun engine(): File = System.getenv("TISMA_HOST_ENGINE")?.let(::File)?.takeIf { it.canExecute() } ?: fake()

    private fun fake(): File = tmp.newFile("engine.py").apply {
        writeBytes(LibraryTest::class.java.getResourceAsStream("/fake_library_engine.py")!!.readBytes())
        setExecutable(true)
    }

    private val cube = buildString {
        appendLine("solid c")
        for (t in listOf(listOf(0f, 0f, 0f, 10f, 0f, 0f, 0f, 10f, 0f), listOf(0f, 0f, 0f, 0f, 10f, 0f, 0f, 0f, 10f),
            listOf(0f, 0f, 0f, 0f, 0f, 10f, 10f, 0f, 0f), listOf(10f, 0f, 0f, 0f, 0f, 10f, 0f, 10f, 0f))) {
            appendLine("facet normal 0 0 1\nouter loop")
            for (k in 0..2) appendLine("vertex ${t[3 * k]} ${t[3 * k + 1]} ${t[3 * k + 2]}")
            appendLine("endloop\nendfacet")
        }
        appendLine("endsolid c")
    }

    private fun idle(cond: () -> Boolean) = compose.waitUntil(10_000) { ShadowLooper.idleMainLooper(); cond() }

    private fun wait(tag: String) = compose.waitUntil(10_000) {
        ShadowLooper.idleMainLooper()
        compose.onAllNodes(hasTestTag(tag)).fetchSemanticsNodes().isNotEmpty()
    }

    /** accessibility_touch.md, rule 9: no control without a name on the screen shown. */
    private fun assertAllNamed(screen: String) {
        val clickable = androidx.compose.ui.test.SemanticsMatcher("clickable") {
            it.config.getOrNull(androidx.compose.ui.semantics.SemanticsActions.OnClick) != null
        }
        val missing = compose.onAllNodes(clickable).fetchSemanticsNodes().filter { n ->
            val c = n.config
            c.getOrNull(androidx.compose.ui.semantics.SemanticsProperties.Text).isNullOrEmpty() &&
                c.getOrNull(androidx.compose.ui.semantics.SemanticsProperties.ContentDescription).isNullOrEmpty() &&
                c.getOrNull(androidx.compose.ui.semantics.SemanticsProperties.EditableText)?.text.isNullOrEmpty() &&
                !(c.getOrNull(androidx.compose.ui.semantics.SemanticsProperties.ToggleableState) != null && n.children.isNotEmpty())
        }.map { it.config.getOrNull(androidx.compose.ui.semantics.SemanticsProperties.TestTag) ?: it.boundsInRoot.toString() }
        assertTrue(missing.isEmpty(), "$screen: controls without a name: $missing")
    }

    @Test
    fun `ML1 ML2 ML10 a sliced part is kept with its recipe, printed again by slicing, and no G-code is stored`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val settings = AppSettings(context)
        val exe = engine()
        val work = tmp.newFolder("work")
        val libDir = tmp.newFolder("library")
        val library = Library(exe, libDir)
        val slice = SliceViewModel(EngineRunner(exe, work), work, saved = settings, library = library)
        val lib = LibraryViewModel(library, tmp.newFolder("libwork"))
        val cloud = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5)
        val printers = PrintersViewModel(PrinterStore(context, MemorySecrets()))
        val nav = Navigator()
        settings.packWizardDone = true
        settings.libraryCapMb = 0 // rule 12: any entry is over this limit, to see the warning
        compose.setContent { TismaTheme { TismaApp(cloud, printers, slice, settings, nav, library = lib) } }
        // The app starts in the library (owner's decision, 05-11-2026); empty at first.
        idle { slice.state.value.libraryAvailable }
        wait("library_empty")

        compose.runOnIdle { nav.select(Section.PLATE) }
        slice.open("cube.stl") { cube.byteInputStream() }
        idle { slice.state.value.modelName != null }
        slice.slice(checkMemory = false)
        idle { slice.state.value.result != null }
        wait("library_save")
        compose.onNodeWithTag("library_save").performScrollTo().performClick()
        idle { slice.state.value.fromLibrary != null }
        val lp = slice.state.value.fromLibrary!!
        assertEquals("cube", lp.name)
        // Rule 1 and ML10: the project and the recipe with its settings and last summary; no G-code anywhere.
        val entry = library.show(lp.entry)
        assertEquals(1, entry.recipes.size)
        assertEquals(50, entry.recipes[0].layers)
        assertTrue(File(libDir, "${lp.entry}/project.3mf").isFile)
        assertTrue(File(libDir, "${lp.entry}/recipes/1/settings.json").isFile)
        // The 256 × 256 picture of the part (rule 1), and the list shows it.
        val thumb = File(libDir, "${lp.entry}/thumb.png")
        assertTrue(thumb.isFile)
        val head = thumb.readBytes().copyOf(8)
        assertTrue(head.copyOfRange(1, 4).contentEquals("PNG".toByteArray()), "a PNG")
        assertEquals(thumb, library.list().single().thumb)
        assertFalse(libDir.walk().any { it.extension in setOf("gcode", "bgcode", "gz") }, "G-code in the library")

        // The list, then «Imprimir» from the entry: it slices again with the recipe (rule 6).
        compose.runOnIdle { nav.select(Section.LIBRARY) }
        wait("library_item_cube")
        assertAllNamed("library list")
        // Rule 12: over the limit it warns, and deletes nothing on its own.
        wait("library_full")
        assertEquals(1, library.list().size)
        compose.onNodeWithTag("library_item_cube").performClick()
        wait("library_print_1")
        // accessibility_touch.md, rule 9: every control of the entry has a name.
        assertAllNamed("library entry")
        // Rule 1, «La imprimí» with a note.
        compose.onNodeWithTag("library_printed").performScrollTo().performClick()
        compose.onNodeWithTag("library_printed_note").performScrollTo().performTextInput("Salió bien")
        compose.onNodeWithTag("library_printed_save").performScrollTo().performClick()
        wait("library_printed_line")
        assertEquals("Salió bien", library.show(lp.entry).printed.single().note)
        File(work, "summary.json").delete()
        compose.onNodeWithTag("library_print_1").performScrollTo().performClick()
        // Sliced again: the summary is written anew (the G-code is not kept in the library).
        idle { nav.section == Section.PLATE && File(work, "summary.json").isFile && slice.state.value.result != null && !slice.state.value.slicing }
        idle { slice.state.value.fromLibrary?.check != null }
        assertTrue(slice.state.value.fromLibrary!!.check!!.sameSettings)
        assertFalse(slice.state.value.fromLibrary!!.check!!.olderEngine)

        // ML2: another setting is a change from the recipe; the next slice keeps its summary in the same entry.
        compose.runOnIdle { slice.setValue("walls.count", kotlinx.serialization.json.JsonPrimitive(4)) }
        idle { slice.state.value.fromLibrary?.check?.sameSettings == false }
        assertEquals(1, library.list().size)
        assertFalse(libDir.walk().any { it.extension in setOf("gcode", "bgcode", "gz") })
    }

    @Test
    fun `ML3 ML4 an analysis holds only while the strength settings stay`() {
        val exe = engine()
        val work = tmp.newFolder("work")
        val library = Library(exe, tmp.newFolder("library"))
        val model = File(work, "cube.stl").apply { writeText(cube) }
        val tea = File(work, "report.json").apply { writeText("{}") }
        fun config(infill: Int, travel: Int) = File(work, "s$infill-$travel.json").apply {
            writeText("""{"infill":{"density":$infill},"speed":{"travel":$travel}}""")
        }
        val e = library.save(null, model, config(20, 150), null, emptyMap(), "Acme", work, analysis = tea,
            analysisSummary = kotlinx.serialization.json.buildJsonObject { put("verdict", kotlinx.serialization.json.JsonPrimitive("holds")) },
            name = "Bracket")
        assertEquals(1, e.analyses.size)
        // ML4: the travel speed does not change the strength.
        assertTrue(library.check(e.id, "1", config(20, 300)).valid("1"))
        // ML3: the infill does.
        val c = library.check(e.id, "1", config(15, 150))
        assertFalse(c.valid("1"))
    }

    @Test
    fun `ML5 ML6 an exported entry imports back, asks before replacing, and an unsafe file is refused`() {
        val exe = engine()
        val work = tmp.newFolder("work")
        val libDir = tmp.newFolder("library")
        val library = Library(exe, libDir)
        val model = File(work, "cube.stl").apply { writeText(cube) }
        val config = File(work, "s.json").apply { writeText("{}") }
        val e = library.save(null, model, config, EngineRunner.ProfileChoice("acme.json#0.4", 0.4, "pla.json", pack = true, name = "Acme One"),
            emptyMap(), "Acme One · PLA", work, name = "Cube")
        val file = library.export(e.id, File(work, "Cube.tismalib"))
        assertTrue(file.length() > 0)
        // Already there: the person chooses (rule 10).
        assertIs<Library.Imported.Exists>(library.import(file))
        val both = assertIs<Library.Imported.Done>(library.import(file, "new")).entry
        assertEquals(2, library.list().size)
        // The recipe's printer choice comes back as it went in.
        assertEquals("Acme One", both.recipes.single().choice?.name)
        assertEquals("pla.json", both.recipes.single().choice?.filament)

        // ML6: a path out of the entry is refused and nothing is written.
        val bad = File(work, "bad.tismalib")
        ZipOutputStream(bad.outputStream()).use { z ->
            z.putNextEntry(ZipEntry("x/../../evil.txt")); z.write("x".toByteArray()); z.closeEntry()
        }
        val before = libDir.walk().count()
        val err = runCatching { library.import(bad) }.exceptionOrNull()
        assertIs<Library.LibraryException>(err)
        assertEquals(2, err.exitCode)
        assertEquals(before, libDir.walk().count())
        assertFalse(File(libDir.parentFile, "evil.txt").exists())
    }
}
