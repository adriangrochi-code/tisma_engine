package net.tisma.app

import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import androidx.lifecycle.compose.LocalLifecycleOwner
import kotlinx.coroutines.delay
import net.tisma.app.ui.WhileVisible
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.util.concurrent.atomic.AtomicInteger
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** Polling (printers, camera, cloud jobs) stops while the app is in the background. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class WhileVisibleTest {
    @get:Rule
    val compose = createComposeRule()

    private class Owner : LifecycleOwner {
        val registry = LifecycleRegistry.createUnsafe(this)
        override val lifecycle: Lifecycle get() = registry
    }

    @Test
    fun `polling stops in the background and starts again on return`() {
        val owner = Owner().apply { registry.currentState = Lifecycle.State.RESUMED }
        val ticks = AtomicInteger()
        compose.mainClock.autoAdvance = false
        compose.setContent {
            CompositionLocalProvider(LocalLifecycleOwner provides owner) {
                WhileVisible { while (true) { ticks.incrementAndGet(); delay(1_000) } }
            }
        }
        compose.mainClock.advanceTimeBy(5_000)
        val visible = ticks.get()
        assertTrue(visible >= 4, "ticks while visible: $visible")
        compose.runOnIdle { owner.registry.currentState = Lifecycle.State.CREATED }
        compose.mainClock.advanceTimeBy(10_000)
        assertEquals(visible, ticks.get(), "no polling in the background")
        compose.runOnIdle { owner.registry.currentState = Lifecycle.State.RESUMED }
        compose.mainClock.advanceTimeBy(3_000)
        assertTrue(ticks.get() > visible)
    }
}
