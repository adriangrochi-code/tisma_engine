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
}
