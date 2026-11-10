package net.tisma.app

import androidx.test.core.app.ApplicationProvider
import net.tisma.app.printers.NetworkDiscovery
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import kotlin.test.assertFalse

/** Discovery made each time the printer list comes to the front: stopping it ends everything it started. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class DiscoveryLifecycleTest {
    @Test
    fun `stopping discovery leaves nothing running`() {
        val before = Thread.getAllStackTraces().keys.count { it.isAlive }
        repeat(20) {
            val d = NetworkDiscovery(ApplicationProvider.getApplicationContext(), {}, { _, _ -> })
            d.start()
            kotlin.test.assertTrue(d.running)
            d.stop()
            assertFalse(d.running)
            assertFalse(d.probing)
        }
        Thread.sleep(200)
        val after = Thread.getAllStackTraces().keys.count { it.isAlive }
        kotlin.test.assertTrue(after - before < 5, "threads left: ${after - before}")
    }
}
