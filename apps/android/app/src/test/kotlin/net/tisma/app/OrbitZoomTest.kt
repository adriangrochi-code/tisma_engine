package net.tisma.app

import net.tisma.app.ui.OrbitCamera
import org.junit.Test
import kotlin.math.abs
import kotlin.test.assertTrue

/** mobile_app.md, rule 6: a double tap zooms in at the point touched, which stays under the finger. */
class OrbitZoomTest {
    private val w = 1080; private val h = 1920

    /** A world point on the screen, in pixels. */
    private fun screen(c: OrbitCamera, p: DoubleArray): Pair<Float, Float> {
        val m = c.viewProjection(w.toFloat() / h)
        val x = m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12]
        val y = m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13]
        val ww = m[3] * p[0] + m[7] * p[1] + m[11] * p[2] + m[15]
        return ((x / ww + 1f) / 2f * w).toFloat() to ((1f - y / ww) / 2f * h).toFloat()
    }

    @Test
    fun `the touched point stays where it was`() {
        val c = OrbitCamera()
        // A point at the depth of the view's centre, off to one side: the centre after moving the view.
        val moved = OrbitCamera().apply { pan(-200f, -300f, h) }
        val p = moved.target.copyOf()
        val (x0, y0) = screen(c, p)
        c.zoomAt(x0, y0, w, h, 1.6f)
        val (x1, y1) = screen(c, p)
        assertTrue(abs(x1 - x0) < 2f && abs(y1 - y0) < 2f, "($x0, $y0) went to ($x1, $y1)")
        // And it really zoomed.
        assertTrue(c.distance < OrbitCamera().distance)
    }
}
