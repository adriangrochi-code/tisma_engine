package net.tisma.app

import android.content.Context
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
import net.tisma.mockcloud.MockCloud
import androidx.compose.ui.test.junit4.createComposeRule
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.shadows.ShadowLooper
import java.io.File
import kotlin.test.assertEquals

/** p2p_library.md, rule 7 bis: a part scanned and sent to print arrives in «Mi biblioteca», ready to slice. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35], qualifiers = "w411dp-h891dp-xxhdpi")
class ScansTest {
    @get:Rule
    val compose = createComposeRule()

    @get:Rule
    val tmp = TemporaryFolder()

    @Test
    fun `a scan sent to print reaches My library and is marked taken`() {
        val server = MockCloud().start()
        try {
            val context: Context = ApplicationProvider.getApplicationContext()
            val settings = AppSettings(context).apply { packWizardDone = true; this.server = server.url }
            val exe = tmp.newFile("engine.py").apply {
                writeBytes(ScansTest::class.java.getResourceAsStream("/fake_library_engine.py")!!.readBytes()); setExecutable(true)
            }
            val work = tmp.newFolder("work")
            val library = Library(exe, tmp.newFolder("library"))
            val slice = SliceViewModel(EngineRunner(exe, work), work, saved = settings, library = library)
            val lib = LibraryViewModel(library, tmp.newFolder("libwork"))
            val cloud = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5)
            val printers = PrintersViewModel(PrinterStore(context, MemorySecrets()))
            val scan = server.addScan("ana@example.com", "Escaneo de la pieza rota", "pieza.stl",
                File("../../../tests/data/cube_ascii.stl").readBytes())
            val nav = Navigator(Section.PLATE)
            compose.setContent { TismaTheme { TismaApp(cloud, printers, slice, settings, nav, library = lib) } }
            compose.runOnIdle { cloud.signIn("ana@example.com", "password1") }
            compose.waitUntil(10_000) { ShadowLooper.idleMainLooper(); cloud.state.value.signedIn && slice.state.value.libraryAvailable }
            compose.runOnIdle { nav.select(Section.LIBRARY) }
            compose.waitUntil(15_000) { ShadowLooper.idleMainLooper(); server.scan(scan.id)?.takenBy != null }
            assertEquals("Test phone (Android)", server.scan(scan.id)?.takenBy)
            val entry = library.list().single()
            assertEquals("Escaneo de la pieza rota", entry.name)
            assertEquals(1, entry.recipes)
        } finally {
            server.stop()
        }
    }
}
