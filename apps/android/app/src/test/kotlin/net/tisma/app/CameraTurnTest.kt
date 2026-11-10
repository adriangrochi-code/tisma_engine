package net.tisma.app

import android.graphics.Bitmap
import android.graphics.Color
import net.tisma.app.ui.cameraTurned
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import kotlin.test.assertEquals
import kotlin.test.assertSame

/** printer_connections.md, «Cámara de la impresora»: the image is turned as the camera is mounted. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class CameraTurnTest {
    @Test
    fun `a turned camera swaps width and height, and an upright one is left alone`() {
        val b = Bitmap.createBitmap(16, 9, Bitmap.Config.ARGB_8888).apply { eraseColor(Color.RED) }
        assertSame(b, cameraTurned(b, false, false, 0))
        val r = cameraTurned(b, true, false, 270)
        assertEquals(9, r.width)
        assertEquals(16, r.height)
        val f = cameraTurned(b, true, true, 180)
        assertEquals(16, f.width)
        assertEquals(9, f.height)
    }
}
