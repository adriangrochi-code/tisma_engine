package net.tisma.app.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.rememberTransformableState
import androidx.compose.foundation.gestures.transformable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import net.tisma.app.R
import net.tisma.app.engine.BinaryGcode
import net.tisma.app.engine.GcodeLayers
import net.tisma.cloud.readUpTo
import java.io.File
import java.util.Locale
import kotlin.math.ceil

/** The colors of the line types (own choice, readable on the dark 3D background of interface.md). */
private fun typeColor(type: String): Color = when (type) {
    "External perimeter" -> Color(0xFFFF8A3D)
    "Perimeter" -> Color(0xFFFFC34D)
    "Overhang perimeter" -> Color(0xFF4D7CFF)
    "Internal infill" -> Color(0xFFC0392B)
    "Solid infill" -> Color(0xFFB04DE0)
    "Top solid infill" -> Color(0xFFF2545B)
    "Bridge infill" -> Color(0xFF5DA9E9)
    "Gap fill" -> Color(0xFFFFFFFF)
    "Ironing" -> Color(0xFFFFB3C1)
    "Skirt/Brim" -> Color(0xFF3DDC97)
    "Support material" -> Color(0xFF2ECC71)
    "Support material interface" -> Color(0xFF1ABC9C)
    "Wipe tower" -> Color(0xFFB3B3B3)
    else -> Color(0xFF8C8C99)
}

/** What the preview colors by. */
enum class PreviewColoring { TYPE, WIDTH, SPEED }

/** A value's color on a blue-green-yellow-red ramp, low to high (own choice, readable on the dark view). */
internal fun rampColor(t: Float): Color {
    val stops = RAMP
    val x = t.coerceIn(0f, 1f) * (stops.size - 1)
    val i = x.toInt().coerceAtMost(stops.size - 2)
    val f = x - i
    val a = stops[i]; val b = stops[i + 1]
    return Color(a.red + (b.red - a.red) * f, a.green + (b.green - a.green) * f, a.blue + (b.blue - a.blue) * f)
}

private val RAMP = listOf(Color(0xFF2B59C3), Color(0xFF1FB5C9), Color(0xFF3DDC97), Color(0xFFFFD23F), Color(0xFFE8452C))

/** A G-code layer by layer, seen from above (mobile_app.md, rules 6 and 12; MA10). Pinch to zoom, drag to move. */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun PreviewScreen(path: String) {
    var layers by remember { mutableStateOf<GcodeLayers?>(null) }
    var failed by remember { mutableStateOf(false) }
    var binary by remember { mutableStateOf(false) }
    LaunchedEffect(path) {
        withContext(Dispatchers.IO) {
            val f = File(path)
            val head = runCatching { f.inputStream().use { it.readUpTo(4) } }.getOrNull()
            when {
                head == null -> failed = true
                head.contentEquals(GcodeLayers.BINARY_MAGIC) -> layers = runCatching {
                    BinaryGcode.open(f).bufferedReader().use { GcodeLayers.read(it) }
                }.onFailure { binary = it is BinaryGcode.Unreadable; failed = true }.getOrNull()
                else -> layers = runCatching { f.bufferedReader().use { GcodeLayers.read(it) } }
                    .onFailure { failed = true }.getOrNull()
            }
        }
    }
    val g = layers
    if (g == null) {
        Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
            when {
                binary -> Text(stringResource(R.string.preview_binary), Modifier.padding(24.dp))
                failed -> Text(stringResource(R.string.error_cannot_read))
                else -> CircularProgressIndicator()
            }
        }
        return
    }
    if (g.layers.isEmpty()) {
        Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) { Text(stringResource(R.string.preview_empty)) }
        return
    }
    var layer by remember(g) { mutableIntStateOf(g.layers.lastIndex) }
    var zoom by remember { mutableFloatStateOf(1f) }
    var pan by remember { mutableStateOf(Offset.Zero) }
    val transform = rememberTransformableState { z, p, _ ->
        zoom = (zoom * z).coerceIn(0.5f, 20f)
        pan += p
    }
    val colors = remember(g) { g.types.map { typeColor(it) } }
    var coloring by remember(g) { mutableStateOf(PreviewColoring.TYPE) }
    var threeD by remember(g) { mutableStateOf(false) }
    val range = when (coloring) {
        PreviewColoring.TYPE -> null
        PreviewColoring.WIDTH -> g.widthRange
        PreviewColoring.SPEED -> g.speedRange
    }
    fun colorOf(l: GcodeLayers.Layer, i: Int): Color {
        val r = range ?: return colors[l.segmentTypes[i].toInt()]
        val v = if (coloring == PreviewColoring.WIDTH) l.widths[i] else l.speeds[i]
        if (v <= 0f) return Color(0xFF8C8C99)
        val span = r.endInclusive - r.start
        return rampColor(if (span < 1e-6f) 0.5f else (v - r.start) / span)
    }
    val muted = Color(0xFF3A3A44)
    Column(Modifier.fillMaxSize()) {
        if (threeD) {
            val bounds = remember(g) {
                val b = g.bounds
                floatArrayOf(b[0], b[1], 0f, b[2], b[3], g.layers.last().z)
            }
            val bed = remember(g) {
                val b = g.bounds
                floatArrayOf(ceil(maxOf(b[2], 10f) / 10f) * 10f, ceil(maxOf(b[3], 10f) / 10f) * 10f)
            }
            var paths by remember(g) { mutableStateOf<FloatArray?>(null) }
            LaunchedEffect(g, layer, coloring) {
                paths = withContext(Dispatchers.Default) { g.lines3d(layer) { l, i -> colorOf(l, i).toArgb() and 0xFFFFFF } }
            }
            Plate3D(null, bed, Modifier.weight(1f).fillMaxWidth().testTag("preview_3d"), paths, bounds)
        } else {
            Box(
                Modifier.weight(1f).fillMaxWidth()
                    .background(Brush.verticalGradient(listOf(Tisma.colors.viewTop, Tisma.colors.viewBottom)))
                    .transformable(transform)
                    .testTag("preview_canvas")
            ) {
                Canvas(Modifier.fillMaxSize()) {
                    val b = g.bounds
                    val w = (b[2] - b[0]).coerceAtLeast(1f)
                    val h = (b[3] - b[1]).coerceAtLeast(1f)
                    val scale = 0.9f * minOf(size.width / w, size.height / h) * zoom
                    val ox = size.width / 2 + pan.x
                    val oy = size.height / 2 + pan.y
                    val cx = (b[0] + b[2]) / 2
                    val cy = (b[1] + b[3]) / 2
                    // Y up, as on the printer's bed.
                    fun px(x: Float) = ox + (x - cx) * scale
                    fun py(y: Float) = oy - (y - cy) * scale
                    val stroke = (0.4f * scale).coerceIn(1f, 12f)
                    if (layer > 0) {
                        val l = g.layers[layer - 1]
                        for (i in 0 until l.count) {
                            val s = l.segments
                            drawLine(muted, Offset(px(s[4 * i]), py(s[4 * i + 1])), Offset(px(s[4 * i + 2]), py(s[4 * i + 3])), stroke)
                        }
                    }
                    val l = g.layers[layer]
                    val s = l.segments
                    for (i in 0 until l.count) {
                        drawLine(colorOf(l, i), Offset(px(s[4 * i]), py(s[4 * i + 1])),
                            Offset(px(s[4 * i + 2]), py(s[4 * i + 3])), stroke)
                    }
                }
            }
        }
        Column(Modifier.padding(16.dp)) {
            Text(stringResource(R.string.preview_layer, layer + 1, g.layers.size,
                String.format(Locale.getDefault(), "%.2f", g.layers[layer].z)), modifier = Modifier.testTag("preview_layer"))
            Slider(value = layer.toFloat(), onValueChange = { layer = it.toInt() },
                valueRange = 0f..g.layers.lastIndex.toFloat().coerceAtLeast(1f),
                steps = (g.layers.size - 2).coerceIn(0, 2000), modifier = Modifier.testTag("preview_slider"))
            FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                listOf(
                    PreviewColoring.TYPE to R.string.preview_by_type,
                    PreviewColoring.WIDTH to R.string.preview_by_width,
                    PreviewColoring.SPEED to R.string.preview_by_speed,
                ).forEach { (c, label) ->
                    val has = c == PreviewColoring.TYPE || (if (c == PreviewColoring.WIDTH) g.widthRange else g.speedRange) != null
                    FilterChip(selected = coloring == c, enabled = has, onClick = { coloring = c },
                        label = { Text(stringResource(label)) }, modifier = Modifier.testTag("preview_by_${c.name.lowercase()}"))
                }
                FilterChip(selected = threeD, onClick = { threeD = !threeD }, label = { Text(stringResource(R.string.preview_3d)) },
                    modifier = Modifier.testTag("preview_3d_toggle"))
            }
            val r = range
            if (r == null) {
                FlowRow(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                    g.types.forEachIndexed { i, t ->
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Box(Modifier.size(10.dp).background(colors[i]))
                            Spacer(Modifier.size(4.dp))
                            Text(t, style = MaterialTheme.typography.bodySmall)
                        }
                    }
                }
            } else {
                val unit = stringResource(if (coloring == PreviewColoring.WIDTH) R.string.unit_mm else R.string.unit_mm_s)
                val fmt = if (coloring == PreviewColoring.WIDTH) "%.2f %s" else "%.0f %s"
                Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.testTag("preview_ramp")) {
                    Text(String.format(Locale.getDefault(), fmt, r.start, unit), style = MaterialTheme.typography.bodySmall)
                    Box(Modifier.weight(1f).padding(horizontal = 8.dp).height(10.dp)
                        .background(Brush.horizontalGradient(RAMP)))
                    Text(String.format(Locale.getDefault(), fmt, r.endInclusive, unit), style = MaterialTheme.typography.bodySmall)
                }
            }
        }
    }
}
