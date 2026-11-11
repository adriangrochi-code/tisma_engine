package net.tisma.app

import android.app.Application
import android.content.Intent
import androidx.test.core.app.ApplicationProvider
import net.tisma.app.ui.openStoreSubscriptions
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.Shadows.shadowOf
import org.robolectric.annotation.Config
import kotlin.test.assertEquals

/** cloud.md 5b: changing or cancelling the plan opens the store's page of this app's subscriptions. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class StoreSubscriptionsTest {
    @Test
    fun `manage opens Google Play's subscriptions of this app`() {
        val app = ApplicationProvider.getApplicationContext<Application>()
        app.openStoreSubscriptions()
        val i = shadowOf(app).nextStartedActivity
        assertEquals(Intent.ACTION_VIEW, i.action)
        assertEquals("https://play.google.com/store/account/subscriptions?package=" + app.packageName, i.dataString)
    }
}
