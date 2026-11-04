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
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.material3.CircularProgressIndicator
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
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import net.tisma.app.R
import net.tisma.app.engine.GcodeLayers
import java.io.File
import java.util.Locale

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
            val head = runCatching { f.inputStream().use { it.readNBytes(4) } }.getOrNull()
            when {
                head == null -> failed = true
                head.contentEquals(GcodeLayers.BINARY_MAGIC) -> binary = true
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
    val muted = Color(0xFF3A3A44)
    Column(Modifier.fillMaxSize()) {
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
                    drawLine(colors[l.segmentTypes[i].toInt()], Offset(px(s[4 * i]), py(s[4 * i + 1])),
                        Offset(px(s[4 * i + 2]), py(s[4 * i + 3])), stroke)
                }
            }
        }
        Column(Modifier.padding(16.dp)) {
            Text(stringResource(R.string.preview_layer, layer + 1, g.layers.size,
                String.format(Locale.getDefault(), "%.2f", g.layers[layer].z)), modifier = Modifier.testTag("preview_layer"))
            Slider(value = layer.toFloat(), onValueChange = { layer = it.toInt() },
                valueRange = 0f..g.layers.lastIndex.toFloat().coerceAtLeast(1f),
                steps = (g.layers.size - 2).coerceIn(0, 2000), modifier = Modifier.testTag("preview_slider"))
            FlowRow(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                g.types.forEachIndexed { i, t ->
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Box(Modifier.size(10.dp).background(colors[i]))
                        Spacer(Modifier.size(4.dp))
                        Text(t, style = MaterialTheme.typography.bodySmall)
                    }
                }
            }
        }
    }
}
