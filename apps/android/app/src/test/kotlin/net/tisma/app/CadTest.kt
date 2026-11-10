package net.tisma.app

import android.content.Context
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performTextReplacement
import androidx.test.core.app.ApplicationProvider
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import net.tisma.app.cloud.CloudViewModel
import net.tisma.app.data.AppSettings
import net.tisma.app.data.KeystoreSecretStore
import net.tisma.app.engine.DesignDoc
import net.tisma.app.engine.EngineRunner
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

/** Design on the phone (cad_design.md): the design in the engine's format, built through the engine. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35], qualifiers = "w411dp-h891dp-xxhdpi")
class CadTest {
    @get:Rule
    val compose = createComposeRule()

    @get:Rule
    val tmp = TemporaryFolder()

    @Test
    fun `a design is written in the engine's format`() {
        var d = DesignDoc()
        d = d.copy(features = d.features + DesignDoc.primitive(d, "box", "Box"))
        d = d.copy(features = d.features + DesignDoc.primitive(d, "cylinder", "Cylinder"))
        d = d.copy(features = d.features + DesignDoc.Operation("Cut 1", "combine", "Box 1", tools = listOf("Cylinder 1"), boolean = "cut"))
        val j = d.json()
        assertEquals("tisma-design-1", j["format"]!!.jsonPrimitive.content)
        val f = j["features"]!!.jsonArray
        assertEquals("box", f[0].jsonObject["kind"]!!.jsonPrimitive.content)
        assertEquals(3, f[0].jsonObject["sizes"]!!.jsonArray.size)
        val op = f[2].jsonObject["operation"]!!.jsonObject
        assertEquals("combine", op["kind"]!!.jsonPrimitive.content)
        assertEquals("Cylinder 1", op["tools"]!!.jsonArray[0].jsonPrimitive.content)
        assertEquals("Box 2", d.freshName("Box").let { if (it == "Box 2") it else "" })
    }

    @Test
    fun `the Design screen adds bodies and an operation, builds them, and puts them on the plate`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val settings = AppSettings(context).apply { packWizardDone = true }
        val bin = tmp.newFolder("lib")
        val cube = File("../../../tests/data/cube_ascii.stl").absoluteFile
        val seen = File(tmp.root, "design_seen.json")
        // An engine that checks and builds (the cube as the bodies), and a SolidCraft beside it.
        val exe = File(bin, "libtisma_engine.so").apply {
            writeText("""#!/bin/sh
                |if [ "${'$'}1" = "design" ] && [ "${'$'}2" = "check" ]; then cp "${'$'}3" '${seen.path}'; echo '{"ok":true,"problems":[],"features":3,"has_body":true}'; exit 0; fi
                |if [ "${'$'}1" = "design" ] && [ "${'$'}2" = "build" ]; then
                |  while [ ${'$'}# -gt 0 ]; do case "${'$'}1" in -o) cp '${cube.path}' "${'$'}2"; shift;; esac; shift; done
                |  echo '{"ok":true,"volume_mm3":1000,"triangles":12,"failed_feature":null,"failure":""}'; exit 0; fi
                |exit 1
                |""".trimMargin())
            setExecutable(true)
        }
        File(bin, "libsolidcraft.so").apply { writeText("#!/bin/sh\n"); setExecutable(true) }
        val work = tmp.newFolder("work")
        val slice = SliceViewModel(EngineRunner(exe, work), work, saved = settings)
        val cloud = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5)
        val printers = PrintersViewModel(PrinterStore(context, MemorySecrets()))
        val nav = Navigator()
        compose.setContent { TismaTheme { TismaApp(cloud, printers, slice, settings, nav) } }
        compose.runOnIdle { nav.select(Section.MORE); nav.open(Screen.Cad) }
        wait("cad_add_box")
        compose.onNodeWithTag("cad_add_box").performClick()
        compose.onNodeWithTag("cad_add_cylinder").performScrollTo().performClick()
        compose.onNodeWithTag("cad_op_combine").performScrollTo().performClick()
        // The cylinder 30 mm tall.
        compose.onNodeWithTag("cad_1_size1").performScrollTo().performTextReplacement("30")
        compose.onNodeWithTag("cad_build").performScrollTo().performClick()
        compose.waitUntil(10_000) { org.robolectric.shadows.ShadowLooper.idleMainLooper(); slice.state.value.designMesh != null }
        val sent = Json.parseToJsonElement(seen.readText()).jsonObject["features"]!!.jsonArray
        assertEquals(3, sent.size)
        assertEquals("30.0", sent[1].jsonObject["sizes"]!!.jsonArray[1].jsonPrimitive.content)
        assertEquals("Cylinder 1", sent[2].jsonObject["operation"]!!.jsonObject["target"]!!.jsonPrimitive.content)
        compose.onNodeWithTag("cad_to_plate").performScrollTo().performClick()
        compose.waitUntil(10_000) { org.robolectric.shadows.ShadowLooper.idleMainLooper(); slice.state.value.mesh != null }
        assertTrue(slice.state.value.modelName!!.endsWith(".stl"))
    }

    /**
     * With the real engine (TISMA_HOST_ENGINE) and the repository's fake
     * SolidCraft (TISMA_FAKE_SOLIDCRAFT, tests/fake_solidcraft_cli.cpp): a box
     * cut by a cylinder is checked and built, and the bodies read back.
     */
    @Test
    fun `the engine checks and builds a design through SolidCraft`() {
        val exe = System.getenv("TISMA_HOST_ENGINE")?.let(::File)
        val sc = System.getenv("TISMA_FAKE_SOLIDCRAFT")?.let(::File)
        org.junit.Assume.assumeTrue(exe != null && exe.canExecute() && sc != null && sc.canExecute())
        var d = DesignDoc("Test")
        d = d.copy(features = d.features + DesignDoc.Primitive("Box 1", "box", listOf(30.0, 20.0, 10.0)))
        d = d.copy(features = d.features + DesignDoc.Primitive("Cylinder 1", "cylinder", listOf(8.0, 30.0)))
        d = d.copy(features = d.features + DesignDoc.Operation("Cut 1", "combine", "Box 1", tools = listOf("Cylinder 1"), boolean = "cut"))
        val runner = EngineRunner(exe!!, tmp.newFolder("work"))
        assertEquals("true", runner.designCheck(d)["ok"].toString())
        val (stl, o) = runner.designBuild(d, sc!!)
        assertEquals("true", o["ok"].toString())
        val mesh = net.tisma.app.engine.MeshFile.read(stl!!)
        assertTrue(mesh.triangles > 12)
        val v = o["volume_mm3"]!!.jsonPrimitive.content.toDouble()
        assertTrue(v > 0 && v < 6000, "volume $v")
        // A cut with no tools is refused by the check, before SolidCraft.
        val bad = d.copy(features = d.features.dropLast(1) + DesignDoc.Operation("Cut 1", "combine", "Box 1", tools = emptyList()))
        println("check of a cut without tools: " + runner.designCheck(bad))
    }

    private fun wait(tag: String) = compose.waitUntil(10_000) {
        org.robolectric.shadows.ShadowLooper.idleMainLooper()
        compose.onAllNodes(hasTestTag(tag), useUnmergedTree = true).fetchSemanticsNodes().isNotEmpty()
    }
}
