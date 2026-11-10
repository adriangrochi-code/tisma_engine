package net.tisma.app

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import net.tisma.app.engine.EngineRunner
import net.tisma.app.engine.SliceViewModel
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.io.File
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull

/** MA9: a model too big for the phone's memory is not sliced; the cloud is offered. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class MemoryTest {
    @Test
    fun `MA9 too big a model offers the cloud instead of slicing`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val work = File(context.cacheDir, "engine")
        val vm = SliceViewModel(EngineRunner(File(work, "none"), work), work, freeMemory = { 10L * 1024 * 1024 })
        val stl = File(System.getProperty("user.dir")).parentFile.parentFile.parentFile.resolve("tests/data/cube_ascii.stl")
        vm.open("cube.stl") { stl.inputStream() }
        eventually { vm.state.value.modelName != null }
        vm.slice()
        val (need, free) = assertNotNull(vm.state.value.tooBig)
        assertEquals(10L * 1024 * 1024, free)
        assertEquals(net.tisma.app.engine.MemoryEstimate.needed(12), need)
        assertFalse(vm.state.value.slicing)
    }

    @Test
    fun `a million triangles fit a mid-range phone, as measured with margin`() {
        // Measured peak on 09-11-2026: 167 MB for a million triangles, 270 MB for two million with supports.
        val million = net.tisma.app.engine.MemoryEstimate.needed(1_000_000)
        assert(million in 300L * 1024 * 1024..512L * 1024 * 1024) { "$million" }
        assert(net.tisma.app.engine.MemoryEstimate.needed(2_000_000) > 270L * 1024 * 1024 * 2)
    }
}
