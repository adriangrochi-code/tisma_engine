package net.tisma.app.ui

import android.opengl.GLES20
import android.opengl.GLSurfaceView
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import net.tisma.app.R
import net.tisma.app.engine.MeshFile
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.FloatBuffer
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.max
import kotlin.math.sin
import kotlin.math.tan

/**
 * An orbit camera around a point of the bed, Z up (the printer's axes). Own
 * design; the matrices are the usual column-major ones of OpenGL.
 */
class OrbitCamera {
    var yaw = -PI / 3
    var pitch = PI / 6
    var distance = 400.0
    var target = doubleArrayOf(110.0, 110.0, 0.0)

    /** Standard views (mobile_app.md, rule 6: 3D, top, front, left, right, back). */
    enum class View { ISO, TOP, FRONT, LEFT, RIGHT, BACK }

    fun set(view: View) {
        when (view) {
            View.ISO -> { yaw = -PI / 3; pitch = PI / 6 }
            View.TOP -> { yaw = -PI / 2; pitch = PI / 2 - 1e-3 }
            View.FRONT -> { yaw = -PI / 2; pitch = 0.0 }
            View.BACK -> { yaw = PI / 2; pitch = 0.0 }
            View.LEFT -> { yaw = PI; pitch = 0.0 }
            View.RIGHT -> { yaw = 0.0; pitch = 0.0 }
        }
    }

    /** Frames a box (min xyz, max xyz) so it fills the view. */
    fun frame(b: FloatArray, fovY: Double = 45.0) {
        target = doubleArrayOf((b[0] + b[3]) / 2.0, (b[1] + b[4]) / 2.0, (b[2] + b[5]) / 2.0)
        val r = 0.5 * kotlin.math.sqrt(((b[3] - b[0]) * (b[3] - b[0]) + (b[4] - b[1]) * (b[4] - b[1]) + (b[5] - b[2]) * (b[5] - b[2])).toDouble())
        distance = max(20.0, 1.15 * r / tan(Math.toRadians(fovY / 2)))
    }

    fun rotate(dxPx: Float, dyPx: Float) {
        yaw -= dxPx * 0.008
        pitch = (pitch + dyPx * 0.008).coerceIn(-PI / 2 + 0.01, PI / 2 - 1e-3)
    }

    fun zoom(factor: Float) {
        distance = (distance / factor).coerceIn(5.0, 5000.0)
    }

    /** Moves the target in the view's plane by pixels on a view [heightPx] high. */
    fun pan(dxPx: Float, dyPx: Float, heightPx: Int) {
        val perPx = 2 * distance * tan(Math.toRadians(22.5)) / max(1, heightPx)
        val rx = -sin(yaw); val ry = cos(yaw) // right vector on the bed
        val ux = -sin(pitch) * cos(yaw); val uy = -sin(pitch) * sin(yaw); val uz = cos(pitch) // up vector
        target[0] += (-dxPx * rx + dyPx * ux) * perPx
        target[1] += (-dxPx * ry + dyPx * uy) * perPx
        target[2] += dyPx * uz * perPx
    }

    fun eye(): DoubleArray = doubleArrayOf(
        target[0] + distance * cos(pitch) * cos(yaw),
        target[1] + distance * cos(pitch) * sin(yaw),
        target[2] + distance * sin(pitch),
    )

    /** projection × view, column-major. */
    fun viewProjection(aspect: Float, fovY: Double = 45.0): FloatArray {
        val e = eye()
        val f = normalize(doubleArrayOf(target[0] - e[0], target[1] - e[1], target[2] - e[2]))
        val up = doubleArrayOf(0.0, 0.0, 1.0)
        val s = normalize(cross(f, up).let { if (it.all { v -> kotlin.math.abs(v) < 1e-9 }) doubleArrayOf(1.0, 0.0, 0.0) else it })
        val u = cross(s, f)
        val view = doubleArrayOf(
            s[0], u[0], -f[0], 0.0,
            s[1], u[1], -f[1], 0.0,
            s[2], u[2], -f[2], 0.0,
            -dot(s, e), -dot(u, e), dot(f, e), 1.0,
        )
        val near = max(0.5, distance / 100)
        val far = distance * 20 + 1000
        val t = 1 / tan(Math.toRadians(fovY / 2))
        val proj = doubleArrayOf(
            t / aspect, 0.0, 0.0, 0.0,
            0.0, t, 0.0, 0.0,
            0.0, 0.0, (far + near) / (near - far), -1.0,
            0.0, 0.0, 2 * far * near / (near - far), 0.0,
        )
        val r = FloatArray(16)
        for (c in 0..3) for (row in 0..3) {
            var sum = 0.0
            for (k in 0..3) sum += proj[k * 4 + row] * view[c * 4 + k]
            r[c * 4 + row] = sum.toFloat()
        }
        return r
    }

    private fun cross(a: DoubleArray, b: DoubleArray) =
        doubleArrayOf(a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])

    private fun dot(a: DoubleArray, b: DoubleArray) = a[0] * b[0] + a[1] * b[1] + a[2] * b[2]

    private fun normalize(a: DoubleArray): DoubleArray {
        val l = kotlin.math.sqrt(dot(a, a))
        return if (l == 0.0) a else doubleArrayOf(a[0] / l, a[1] / l, a[2] / l)
    }
}

/** Draws the bed (a grid with the X, Y and Z axes at the origin) and the model, flat-shaded. GLES 2.0. */
private class PlateRenderer(val camera: OrbitCamera) : GLSurfaceView.Renderer {
    @Volatile var mesh: MeshFile? = null
        set(v) { field = v; meshDirty = true }
    @Volatile var bed = floatArrayOf(220f, 220f)
        set(v) { field = v; bedDirty = true }
    /** Marked faces (Engineering): triangles as x y z per corner, and their color (0xRRGGBB). */
    @Volatile var marks: List<Pair<FloatArray, Int>> = emptyList()
        set(v) { field = v; marksDirty = true }
    @Volatile private var marksDirty = true
    private var marksBuf: List<Triple<FloatBuffer, Int, Int>> = emptyList()
    var aspectNow = 1f
        private set
    /** Toolpaths as lines, x y z r g b per vertex (GcodeLayers.lines3d), drawn in their own colors. */
    @Volatile var paths: FloatArray? = null
        set(v) { field = v; pathsDirty = true }
    @Volatile private var pathsDirty = true
    private var pathsBuf: FloatBuffer? = null
    private var pathsCount = 0
    @Volatile private var meshDirty = true
    @Volatile private var bedDirty = true
    private var program = 0
    private var aspect = 1f
    private var height = 1
    private var meshBuf: FloatBuffer? = null
    private var meshCount = 0
    private var gridBuf: FloatBuffer? = null
    private var gridCount = 0
    val viewHeight get() = height

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
        program = link("""
            uniform mat4 uMvp;
            attribute vec3 aPos;
            attribute vec3 aNormal;
            attribute vec3 aColor;
            varying float vLight;
            varying vec3 vColor;
            void main() {
                gl_Position = uMvp * vec4(aPos, 1.0);
                vColor = aColor;
                vec3 l = normalize(vec3(-0.4, -0.6, 0.7));
                vLight = 0.35 + 0.65 * abs(dot(normalize(aNormal), l));
            }
        """, """
            precision mediump float;
            uniform vec4 uColor;
            uniform float uLit;
            uniform float uOwnColor;
            varying float vLight;
            varying vec3 vColor;
            void main() {
                vec3 c = mix(uColor.rgb, vColor, uOwnColor);
                gl_FragColor = vec4(c * mix(1.0, vLight, uLit), uColor.a);
            }
        """)
        GLES20.glEnable(GLES20.GL_DEPTH_TEST)
        // The 3D view's dark background of interface.md, the same in both themes.
        GLES20.glClearColor(0.10f, 0.10f, 0.12f, 1f)
        meshDirty = true
        bedDirty = true
        pathsDirty = true
    }

    override fun onSurfaceChanged(gl: GL10?, w: Int, h: Int) {
        GLES20.glViewport(0, 0, w, h)
        aspect = w.toFloat() / max(1, h)
        aspectNow = aspect
        height = h
    }

    override fun onDrawFrame(gl: GL10?) {
        if (bedDirty) { buildGrid(); bedDirty = false }
        if (meshDirty) { buildMesh(); meshDirty = false }
        if (marksDirty) {
            marksBuf = marks.map { (tris, c) ->
                val n = tris.size / 9
                val data = FloatArray(n * 3 * 6)
                for (t in 0 until n) {
                    val i = 9 * t
                    val ux = tris[i + 3] - tris[i]; val uy = tris[i + 4] - tris[i + 1]; val uz = tris[i + 5] - tris[i + 2]
                    val vx = tris[i + 6] - tris[i]; val vy = tris[i + 7] - tris[i + 1]; val vz = tris[i + 8] - tris[i + 2]
                    val nx = uy * vz - uz * vy; val ny = uz * vx - ux * vz; val nz = ux * vy - uy * vx
                    for (k in 0..2) {
                        val o = (3 * t + k) * 6
                        data[o] = tris[i + 3 * k]; data[o + 1] = tris[i + 3 * k + 1]; data[o + 2] = tris[i + 3 * k + 2]
                        data[o + 3] = nx; data[o + 4] = ny; data[o + 5] = nz
                    }
                }
                Triple(buffer(data), n * 3, c)
            }
            marksDirty = false
        }
        if (pathsDirty) { paths.let { p -> pathsBuf = p?.let { buffer(it) }; pathsCount = (p?.size ?: 0) / 6 }; pathsDirty = false }
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT or GLES20.GL_DEPTH_BUFFER_BIT)
        GLES20.glUseProgram(program)
        val mvp = camera.viewProjection(aspect)
        GLES20.glUniformMatrix4fv(GLES20.glGetUniformLocation(program, "uMvp"), 1, false, mvp, 0)
        val pos = GLES20.glGetAttribLocation(program, "aPos")
        val nor = GLES20.glGetAttribLocation(program, "aNormal")
        val color = GLES20.glGetUniformLocation(program, "uColor")
        val lit = GLES20.glGetUniformLocation(program, "uLit")
        val own = GLES20.glGetUniformLocation(program, "uOwnColor")
        val col = GLES20.glGetAttribLocation(program, "aColor")
        GLES20.glUniform1f(own, 0f)
        GLES20.glDisableVertexAttribArray(col)
        gridBuf?.let { g ->
            GLES20.glUniform1f(lit, 0f)
            GLES20.glDisableVertexAttribArray(nor)
            GLES20.glVertexAttrib3f(nor, 0f, 0f, 1f)
            g.position(0)
            GLES20.glEnableVertexAttribArray(pos)
            GLES20.glVertexAttribPointer(pos, 3, GLES20.GL_FLOAT, false, 24, g)
            // grid gray (#2B2B31 lifted to be seen), then the axes red, green, blue.
            GLES20.glUniform4f(color, 0.24f, 0.24f, 0.28f, 1f)
            GLES20.glDrawArrays(GLES20.GL_LINES, 0, gridCount - 6)
            listOf(floatArrayOf(0.9f, 0.2f, 0.2f), floatArrayOf(0.2f, 0.8f, 0.3f), floatArrayOf(0.3f, 0.5f, 1f)).forEachIndexed { i, c ->
                GLES20.glUniform4f(color, c[0], c[1], c[2], 1f)
                GLES20.glDrawArrays(GLES20.GL_LINES, gridCount - 6 + 2 * i, 2)
            }
        }
        meshBuf?.let { m ->
            GLES20.glUniform1f(lit, 1f)
            // The selected part's violet (interface.md accent), lighter to read the shading.
            GLES20.glUniform4f(color, 0.62f, 0.38f, 0.92f, 1f)
            m.position(0)
            GLES20.glEnableVertexAttribArray(pos)
            GLES20.glVertexAttribPointer(pos, 3, GLES20.GL_FLOAT, false, 24, m)
            m.position(3)
            GLES20.glEnableVertexAttribArray(nor)
            GLES20.glVertexAttribPointer(nor, 3, GLES20.GL_FLOAT, false, 24, m)
            GLES20.glDrawArrays(GLES20.GL_TRIANGLES, 0, meshCount)
        }
        if (marksBuf.isNotEmpty()) {
            // On top of the model's faces: pulled toward the eye so they are not hidden by them.
            GLES20.glEnable(GLES20.GL_POLYGON_OFFSET_FILL)
            GLES20.glPolygonOffset(-1f, -2f)
            GLES20.glUniform1f(lit, 1f)
            for ((b, count, c) in marksBuf) {
                GLES20.glUniform4f(color, (c shr 16 and 0xFF) / 255f, (c shr 8 and 0xFF) / 255f, (c and 0xFF) / 255f, 1f)
                b.position(0)
                GLES20.glEnableVertexAttribArray(pos)
                GLES20.glVertexAttribPointer(pos, 3, GLES20.GL_FLOAT, false, 24, b)
                b.position(3)
                GLES20.glEnableVertexAttribArray(nor)
                GLES20.glVertexAttribPointer(nor, 3, GLES20.GL_FLOAT, false, 24, b)
                GLES20.glDrawArrays(GLES20.GL_TRIANGLES, 0, count)
            }
            GLES20.glDisable(GLES20.GL_POLYGON_OFFSET_FILL)
        }
        pathsBuf?.takeIf { pathsCount > 0 }?.let { p ->
            GLES20.glUniform1f(lit, 0f)
            GLES20.glUniform1f(own, 1f)
            GLES20.glUniform4f(color, 1f, 1f, 1f, 1f)
            GLES20.glDisableVertexAttribArray(nor)
            GLES20.glVertexAttrib3f(nor, 0f, 0f, 1f)
            p.position(0)
            GLES20.glEnableVertexAttribArray(pos)
            GLES20.glVertexAttribPointer(pos, 3, GLES20.GL_FLOAT, false, 24, p)
            p.position(3)
            GLES20.glEnableVertexAttribArray(col)
            GLES20.glVertexAttribPointer(col, 3, GLES20.GL_FLOAT, false, 24, p)
            GLES20.glDrawArrays(GLES20.GL_LINES, 0, pathsCount)
            GLES20.glDisableVertexAttribArray(col)
        }
    }

    private fun buildGrid() {
        val (w, d) = bed[0] to bed[1]
        val lines = ArrayList<Float>()
        fun line(x0: Float, y0: Float, z0: Float, x1: Float, y1: Float, z1: Float) {
            lines += listOf(x0, y0, z0, 0f, 0f, 1f, x1, y1, z1, 0f, 0f, 1f)
        }
        var x = 0f
        while (x <= w + 0.01f) { line(x, 0f, 0f, x, d, 0f); x += 10f }
        var y = 0f
        while (y <= d + 0.01f) { line(0f, y, 0f, w, y, 0f); y += 10f }
        line(0f, 0f, 0.05f, 30f, 0f, 0.05f)
        line(0f, 0f, 0.05f, 0f, 30f, 0.05f)
        line(0f, 0f, 0f, 0f, 0f, 30f)
        gridBuf = buffer(lines.toFloatArray())
        gridCount = lines.size / 6
    }

    private fun buildMesh() {
        val m = mesh
        if (m == null) { meshBuf = null; meshCount = 0; return }
        val data = FloatArray(m.triangles * 3 * 6)
        for (t in 0 until m.triangles) for (k in 0..2) {
            val o = (3 * t + k) * 6
            data[o] = m.positions[9 * t + 3 * k]; data[o + 1] = m.positions[9 * t + 3 * k + 1]; data[o + 2] = m.positions[9 * t + 3 * k + 2]
            data[o + 3] = m.normals[3 * t]; data[o + 4] = m.normals[3 * t + 1]; data[o + 5] = m.normals[3 * t + 2]
        }
        meshBuf = buffer(data)
        meshCount = m.triangles * 3
    }

    private fun buffer(a: FloatArray): FloatBuffer =
        ByteBuffer.allocateDirect(a.size * 4).order(ByteOrder.nativeOrder()).asFloatBuffer().apply { put(a); position(0) }

    private fun link(vs: String, fs: String): Int {
        fun shader(type: Int, src: String) = GLES20.glCreateShader(type).also { GLES20.glShaderSource(it, src); GLES20.glCompileShader(it) }
        return GLES20.glCreateProgram().also { p ->
            GLES20.glAttachShader(p, shader(GLES20.GL_VERTEX_SHADER, vs))
            GLES20.glAttachShader(p, shader(GLES20.GL_FRAGMENT_SHADER, fs))
            GLES20.glLinkProgram(p)
        }
    }
}

/**
 * The plate's 3D view (mobile_app.md, rule 6): the bed and the model fill it; one
 * finger turns, two move, pinching zooms, a double tap zooms in; the view
 * buttons and «Frame» along the bottom.
 */
@Composable
fun Plate3D(
    mesh: MeshFile?, bed: FloatArray, modifier: Modifier = Modifier,
    paths: FloatArray? = null, pathBounds: FloatArray? = null,
    marks: List<Pair<FloatArray, Int>> = emptyList(),
    /** With it, a tap gives the ray under the finger (origin, direction) instead of zooming. */
    onPick: ((FloatArray, FloatArray) -> Unit)? = null,
    /** One finger does not turn the view (QA AND9: while marking, the page scrolls and a tap marks); two still move and zoom. */
    lockRotation: Boolean = false,
) {
    val locked = androidx.compose.runtime.rememberUpdatedState(lockRotation)
    val camera = remember { OrbitCamera() }
    val renderer = remember { PlateRenderer(camera) }
    val holder = remember { arrayOfNulls<GLSurfaceView>(1) }
    fun frame() {
        val b = pathBounds ?: mesh?.bounds ?: floatArrayOf(0f, 0f, 0f, bed[0], bed[1], 0f)
        camera.frame(b)
        holder[0]?.requestRender()
    }
    DisposableEffect(marks) {
        renderer.marks = marks
        holder[0]?.requestRender()
        onDispose { }
    }
    val pickNow = androidx.compose.runtime.rememberUpdatedState(onPick)
    DisposableEffect(paths) {
        renderer.paths = paths
        holder[0]?.requestRender()
        onDispose { }
    }
    DisposableEffect(mesh, bed[0], bed[1], pathBounds) {
        renderer.bed = bed
        renderer.mesh = mesh
        frame()
        onDispose { }
    }
    // The GL thread follows the screen's life (GLSurfaceView's documented contract): paused when the app is
    // not shown, so the main thread never waits on a frame while another screen or app takes over (QA AND6).
    val lifecycle = androidx.lifecycle.compose.LocalLifecycleOwner.current.lifecycle
    DisposableEffect(lifecycle) {
        val observer = androidx.lifecycle.LifecycleEventObserver { _, e ->
            when (e) {
                androidx.lifecycle.Lifecycle.Event.ON_PAUSE -> holder[0]?.onPause()
                androidx.lifecycle.Lifecycle.Event.ON_RESUME -> holder[0]?.onResume()
                else -> {}
            }
        }
        lifecycle.addObserver(observer)
        onDispose { lifecycle.removeObserver(observer) }
    }
    Box(modifier.testTag("plate_3d")) {
        AndroidView(
            factory = { ctx ->
                GLSurfaceView(ctx).apply {
                    setEGLContextClientVersion(2)
                    setRenderer(renderer)
                    renderMode = GLSurfaceView.RENDERMODE_WHEN_DIRTY
                    holder[0] = this
                }
            },
            modifier = Modifier.matchParentSize()
                .pointerInput(Unit) {
                    detectTapGestures(
                        onDoubleTap = { camera.zoom(1.6f); holder[0]?.requestRender() },
                        onTap = { at ->
                            val pick = pickNow.value ?: return@detectTapGestures
                            val x = 2f * at.x / size.width - 1f
                            val y = 1f - 2f * at.y / size.height
                            val (o, d) = net.tisma.app.engine.Picking.ray(camera.viewProjection(renderer.aspectNow), x, y)
                            pick(o, d)
                        },
                    )
                }
                .pointerInput(Unit) {
                    awaitEachGesture {
                        awaitFirstDown(requireUnconsumed = false)
                        var lastSpan = 0f
                        var lastCentroid: androidx.compose.ui.geometry.Offset? = null
                        var lastCount = 0
                        while (true) {
                            val event = awaitPointerEvent()
                            val pressed = event.changes.filter { it.pressed }
                            if (pressed.isEmpty()) break
                            val c = pressed.fold(androidx.compose.ui.geometry.Offset.Zero) { a, p -> a + p.position } / pressed.size.toFloat()
                            val span = if (pressed.size >= 2) (pressed[0].position - pressed[1].position).getDistance() else 0f
                            var moved = false
                            if (pressed.size == lastCount && lastCentroid != null) {
                                val d = c - lastCentroid
                                moved = (d.getDistance() > 0f || (lastSpan > 0f && span != lastSpan)) &&
                                    !(pressed.size == 1 && locked.value)
                                if (pressed.size == 1) { if (!locked.value) camera.rotate(d.x, d.y) }
                                else {
                                    camera.pan(d.x, d.y, renderer.viewHeight)
                                    if (lastSpan > 0f && span > 0f) camera.zoom(span / lastSpan)
                                }
                                holder[0]?.requestRender()
                            }
                            lastCentroid = c
                            lastSpan = span
                            lastCount = pressed.size
                            // Only movement is taken, so a still tap reaches the tap detector (picking, double tap).
                            if (moved) pressed.forEach { it.consume() }
                        }
                    }
                },
        )
        Row(Modifier.align(Alignment.BottomCenter).padding(4.dp)) {
            listOf(OrbitCamera.View.ISO to R.drawable.tisma_view, OrbitCamera.View.TOP to R.drawable.tisma_view_top,
                OrbitCamera.View.FRONT to R.drawable.tisma_view_front, OrbitCamera.View.LEFT to R.drawable.tisma_view_left,
                OrbitCamera.View.RIGHT to R.drawable.tisma_view_right, OrbitCamera.View.BACK to R.drawable.tisma_view_back).forEach { (v, icon) ->
                IconButton(onClick = { camera.set(v); frame() }, modifier = Modifier.size(40.dp)) {
                    Icon(painterResource(icon), v.name, tint = Color(0xFFECECF0))
                }
            }
            IconButton(onClick = { frame() }, modifier = Modifier.size(40.dp).testTag("frame")) {
                Icon(painterResource(R.drawable.tisma_zoom_fit), stringResource(R.string.plate_frame), tint = Color(0xFFECECF0))
            }
        }
    }
}
