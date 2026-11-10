package net.tisma.app

import android.app.Application
import android.content.Intent
import android.net.Uri
import androidx.test.core.app.ApplicationProvider
import net.tisma.app.ui.shareGcode
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.Shadows.shadowOf
import org.robolectric.annotation.Config
import java.io.File
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

/** mobile_app.md, rule 6: the sliced G-code can be shared with another app, not only saved or sent. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class ShareGcodeTest {
    @Test
    fun `the G-code goes to Android's chooser as a stream`() {
        val app = ApplicationProvider.getApplicationContext<Application>()
        val gcode = File(app.cacheDir, "engine/cube.gcode").apply { parentFile?.mkdirs(); writeText("G28\n") }
        shareGcode(app, gcode)
        val chooser = assertNotNull(shadowOf(app).nextStartedActivity)
        assertEquals(Intent.ACTION_CHOOSER, chooser.action)
        @Suppress("DEPRECATION")
        val send = assertNotNull(chooser.getParcelableExtra<Intent>(Intent.EXTRA_INTENT))
        assertEquals(Intent.ACTION_SEND, send.action)
        @Suppress("DEPRECATION")
        val uri = assertNotNull(send.getParcelableExtra<Uri>(Intent.EXTRA_STREAM))
        assertTrue(uri.toString().endsWith("cube.gcode"))
        assertEquals("G28\n", File(app.cacheDir, "share/cube.gcode").readText())
    }
}
