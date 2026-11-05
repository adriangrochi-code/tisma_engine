package net.tisma.app

import android.content.Intent
import android.net.Uri
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** MA5: «Share with Tisma» an STL from another app opens it on the plate. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class ShareTest {
    @Test
    fun `the shared file is taken from a Send or a View intent`() {
        val uri = Uri.parse("content://files/cube.stl")
        assertEquals(uri, sharedUri(Intent(Intent.ACTION_SEND).putExtra(Intent.EXTRA_STREAM, uri)))
        assertEquals(uri, sharedUri(Intent(Intent.ACTION_VIEW, uri)))
        assertNull(sharedUri(Intent(Intent.ACTION_MAIN)))
    }

    @Test
    fun `MA5 the app accepts an STL shared from another app`() {
        val send = Intent(Intent.ACTION_SEND).setType("model/stl")
        val handlers = androidx.test.core.app.ApplicationProvider.getApplicationContext<android.app.Application>()
            .packageManager.queryIntentActivities(send, 0)
        assertTrue(handlers.any { it.activityInfo.name == MainActivity::class.java.name })
        // And the app starts with such an intent without failing.
        Robolectric.buildActivity(MainActivity::class.java,
            Intent(Intent.ACTION_SEND).setType("model/stl").putExtra(Intent.EXTRA_STREAM, Uri.parse("content://none/cube.stl")))
            .setup()
    }
}
