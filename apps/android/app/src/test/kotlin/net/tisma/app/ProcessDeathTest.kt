package net.tisma.app

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import net.tisma.app.data.AppSettings
import net.tisma.app.engine.EngineRunner
import net.tisma.app.engine.SliceViewModel
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.shadows.ShadowLooper
import java.io.File
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull

/** Android ends the app in the background (the camera, the file manager): the plate comes back with its model. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class ProcessDeathTest {
    @get:Rule
    val tmp = TemporaryFolder()

    private fun until(c: () -> Boolean) {
        val end = System.currentTimeMillis() + 10_000
        while (!c() && System.currentTimeMillis() < end) { ShadowLooper.idleMainLooper(); Thread.sleep(10) }
    }

    @Test
    fun `the plate's model comes back after the process ends, and not after it was taken off`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val settings = AppSettings(context)
        val work = tmp.newFolder("work")
        fun vm() = SliceViewModel(EngineRunner(File(work, "none"), work), work, saved = settings)
        val first = vm()
        first.open("cube.stl") { File("../../../tests/data/cube_ascii.stl").inputStream() }
        until { first.state.value.mesh != null }
        // A new process: a new view model with the same settings and folder.
        val again = vm()
        until { again.state.value.mesh != null }
        assertEquals("cube.stl", again.state.value.modelName)
        assertNotNull(again.state.value.mesh)
        again.removeModel()
        val third = vm()
        ShadowLooper.idleMainLooper()
        assertNull(third.state.value.modelName)
    }

    @Test
    fun `a part opened from My library comes back as that part, not as a project`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val settings = AppSettings(context)
        val exe = tmp.newFile("engine.py").apply {
            writeBytes(ProcessDeathTest::class.java.getResourceAsStream("/fake_library_engine.py")!!.readBytes()); setExecutable(true)
        }
        val library = net.tisma.app.engine.Library(exe, tmp.newFolder("library"))
        val stl = File("../../../tests/data/cube_ascii.stl").absoluteFile
        val entry = library.save(null, stl, tmp.newFile("c.json").apply { writeText("{}") }, null, emptyMap(), "", tmp.newFolder("s"), name = "Soporte")
        val work = tmp.newFolder("work")
        fun vm() = SliceViewModel(EngineRunner(exe, work), work, saved = settings, library = library)
        val first = vm()
        first.openFromLibrary(library.show(entry.id))
        until { first.state.value.modelName == "Soporte" }
        val again = vm()
        until { again.state.value.modelName != null }
        assertEquals("Soporte", again.state.value.modelName)
        assertEquals(false, again.state.value.isProject)
    }
}
