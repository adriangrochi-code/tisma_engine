package net.tisma.app

import android.content.Context
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.test.core.app.ApplicationProvider
import net.tisma.app.cloud.CloudViewModel
import net.tisma.app.data.AppSettings
import net.tisma.app.data.KeystoreSecretStore
import net.tisma.app.engine.EngineRunner
import net.tisma.app.engine.SiteLink
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
import java.net.URLEncoder
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream
import kotlin.test.assertEquals
import kotlin.test.assertIs
import kotlin.test.assertTrue

/** mobile_app.md, rule 10: a model site's link, asked before anything is downloaded. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35], qualifiers = "w411dp-h891dp-xxhdpi")
class SiteLinkTest {
    @get:Rule
    val compose = createComposeRule()

    @get:Rule
    val tmp = TemporaryFolder()

    private fun link(scheme: String, url: String) = "$scheme://open?file=" + URLEncoder.encode(url, "UTF-8")

    @Test
    fun `only https links to the listed sites and model files are taken`() {
        val ok = assertIs<SiteLink.Parsed.Ok>(SiteLink.parse(link("prusaslicer", "https://files.printables.com/media/prints/1/stl/bracket.stl")))
        assertEquals("bracket.stl", ok.request.fileName)
        assertEquals("files.printables.com", ok.request.site)
        assertIs<SiteLink.Parsed.Ok>(SiteLink.parse(link("tisma", "https://www.thingiverse.com/download:9/part.3mf")))
        fun refused(l: String) = assertIs<SiteLink.Parsed.Refused>(SiteLink.parse(l)).reason
        assertEquals(SiteLink.Reason.NOT_HTTPS, refused(link("tisma", "http://files.printables.com/a.stl")))
        assertEquals(SiteLink.Reason.SITE, refused(link("tisma", "https://evil.example/a.stl")))
        assertEquals(SiteLink.Reason.SITE, refused(link("tisma", "https://printables.com.evil.example/a.stl")))
        assertEquals(SiteLink.Reason.KIND, refused(link("tisma", "https://files.printables.com/a.exe")))
        assertEquals(SiteLink.Reason.NOT_A_LINK, refused("tisma://other?file=x"))
    }

    @Test
    fun `a ZIP gives its first model, by its name only`() {
        val zip = tmp.newFile("m.zip")
        ZipOutputStream(zip.outputStream()).use { z ->
            z.putNextEntry(ZipEntry("readme.txt")); z.write("hi".toByteArray()); z.closeEntry()
            z.putNextEntry(ZipEntry("../../files/cube.stl")); z.write("solid c\nendsolid c\n".toByteArray()); z.closeEntry()
        }
        val dir = tmp.newFolder("out")
        val m = SiteLink.unzipModel(zip, dir)
        assertEquals(File(dir, "cube.stl"), m)
        assertTrue(m.readText().startsWith("solid"))
    }

    @Test
    fun `the link asks first, offers to allow downloads, and the model reaches the plate`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val settings = AppSettings(context).apply { packWizardDone = true; allowSiteDownloads = false }
        val work = tmp.newFolder("work")
        val slice = SliceViewModel(EngineRunner(File(work, "none"), work), work, saved = settings)
        val cloud = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5)
        val printers = PrintersViewModel(PrinterStore(context, MemorySecrets()))
        compose.setContent { TismaTheme { TismaApp(cloud, printers, slice, settings, Navigator(Section.PLATE)) } }
        compose.runOnIdle { slice.linkOpened(link("prusaslicer", "https://files.printables.com/media/cube.stl")) }
        compose.waitUntil(10_000) { ShadowLooper.idleMainLooper(); compose.onAllNodes(hasTestTag("site_open")).fetchSemanticsNodes().isNotEmpty() }
        // Nothing is downloaded before the yes; with downloads off, the dialog offers to allow them.
        assertEquals("cube.stl", slice.state.value.siteLink?.fileName)
        compose.onNodeWithTag("site_open").assertExists()
        compose.runOnIdle {
            settings.allowSiteDownloads = true
            slice.acceptSiteLink { r, dir -> File(dir.apply { mkdirs() }, r.fileName).apply { writeText(File("../../../tests/data/cube_ascii.stl").readText()) } }
        }
        compose.waitUntil(10_000) { ShadowLooper.idleMainLooper(); slice.state.value.modelName == "cube.stl" }
    }
}
