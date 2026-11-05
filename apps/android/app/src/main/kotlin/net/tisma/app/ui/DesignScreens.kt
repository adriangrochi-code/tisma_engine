package net.tisma.app.ui

import android.content.Context
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Button
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.booleanOrNull
import kotlinx.serialization.json.doubleOrNull
import kotlinx.serialization.json.intOrNull
import net.tisma.app.R
import net.tisma.app.engine.SliceViewModel

/** The fasteners the engine makes (src/cad/fasteners), and the metric sizes it knows. */
private val FASTENERS = listOf("hex_bolt", "socket_bolt", "thumb_screw", "hex_nut", "thumb_nut", "washer", "threaded_rod", "standoff", "spacer")
private val SIZES = listOf("M2", "M2.5", "M3", "M4", "M5", "M6", "M8", "M10", "M12")

/**
 * Design on the phone: the engine's own parts, gears and metric fasteners
 * (`part gear|fastener`; the desktop's Design parts, design_parts.md). The
 * spec starts from the engine's defaults; every number and switch of it can
 * be changed; «Make» puts the part on the plate. A spec the engine refuses
 * shows its reason. Own design over the engine's spec format.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun PartsScreen(slice: SliceViewModel, nav: Navigator) {
    var kind by rememberSaveable { mutableStateOf("gear") }
    val values = remember(kind) { mutableStateMapOf<String, JsonElement>() }
    var error by remember { mutableStateOf<String?>(null) }
    var busy by remember { mutableStateOf(false) }
    LaunchedEffect(kind) { slice.partDefaults(kind) { o -> values.clear(); o?.let { values.putAll(it) } } }
    ScreenColumn {
        FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            listOf("gear" to R.string.part_gear, "fastener" to R.string.part_fastener).forEach { (k, l) ->
                FilterChip(selected = kind == k, onClick = { kind = k; error = null }, label = { Text(stringResource(l)) },
                    modifier = Modifier.testTag("part_$k"))
            }
        }
        if (kind == "fastener") {
            Text(stringResource(R.string.part_kind), style = MaterialTheme.typography.labelLarge)
            FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                FASTENERS.forEach { f ->
                    FilterChip(selected = (values["kind"] as? JsonPrimitive)?.content == f, onClick = { values["kind"] = JsonPrimitive(f) },
                        label = { Text(fastenerName(f)) }, modifier = Modifier.testTag("fastener_$f"))
                }
            }
            Text(stringResource(R.string.part_size), style = MaterialTheme.typography.labelLarge)
            FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                SIZES.forEach { z ->
                    FilterChip(selected = (values["size"] as? JsonPrimitive)?.content == z, onClick = { values["size"] = JsonPrimitive(z) },
                        label = { Text(z) }, modifier = Modifier.testTag("size_$z"))
                }
            }
        }
        values.keys.filter { it !in setOf("format", "kind", "size") }.sorted().forEach { key ->
            val v = values[key] as? JsonPrimitive ?: return@forEach
            when {
                v.booleanOrNull != null && !v.isString -> Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(partLabel(key), modifier = Modifier.weight(1f))
                    Switch(v.booleanOrNull == true, { values[key] = JsonPrimitive(it) },
                        modifier = Modifier.testTag("spec_$key").named(partLabel(key)))
                }
                v.doubleOrNull != null && !v.isString -> {
                    val integer = v.intOrNull != null && !v.content.contains('.')
                    var text by remember(kind, key) { mutableStateOf(v.content.removeSuffix(".0")) }
                    OutlinedTextField(text, { t ->
                        text = t
                        t.replace(',', '.').toDoubleOrNull()?.let { d -> values[key] = if (integer) JsonPrimitive(d.toLong()) else JsonPrimitive(d) }
                    }, singleLine = true, label = { Text(partLabel(key)) },
                        keyboardOptions = KeyboardOptions(keyboardType = if (integer) KeyboardType.Number else KeyboardType.Decimal),
                        modifier = Modifier.fillMaxWidth().testTag("spec_$key"))
                }
            }
        }
        error?.let { Text(it, color = MaterialTheme.colorScheme.error, modifier = Modifier.testTag("part_error")) }
        Button(onClick = {
            busy = true; error = null
            slice.makePart(kind, JsonObject(values.toMap())) { e ->
                busy = false; error = e
                if (e == null) nav.select(Section.PLATE)
            }
        }, enabled = !busy && values.isNotEmpty(), modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp).testTag("part_make")) {
            Text(stringResource(R.string.part_make))
        }
    }
}

@Composable
private fun partLabel(key: String): String {
    val id = when (key) {
        "teeth" -> R.string.spec_teeth; "module" -> R.string.spec_module; "width" -> R.string.spec_width
        "bore" -> R.string.spec_bore; "pressure_angle" -> R.string.spec_pressure_angle; "helix_angle" -> R.string.spec_helix_angle
        "herringbone" -> R.string.spec_herringbone; "left_hand" -> R.string.spec_left_hand; "rack" -> R.string.spec_rack
        "rack_base" -> R.string.spec_rack_base; "backlash" -> R.string.spec_backlash; "length" -> R.string.spec_length
        "clearance" -> R.string.spec_clearance; "diameter" -> R.string.spec_diameter; "pitch" -> R.string.spec_pitch
        "segments" -> R.string.spec_segments
        else -> null
    }
    return id?.let { stringResource(it) } ?: key
}

@Composable
private fun fastenerName(id: String): String = stringResource(when (id) {
    "hex_bolt" -> R.string.fastener_hex_bolt; "socket_bolt" -> R.string.fastener_socket_bolt; "thumb_screw" -> R.string.fastener_thumb_screw
    "hex_nut" -> R.string.fastener_hex_nut; "thumb_nut" -> R.string.fastener_thumb_nut; "washer" -> R.string.fastener_washer
    "threaded_rod" -> R.string.fastener_threaded_rod; "standoff" -> R.string.fastener_standoff
    else -> R.string.fastener_spacer
})

/** The FDM calibration tests the engine makes (calibrations.md; `tisma-engine calibration`), in a useful order. */
val CALIBRATIONS = listOf("first_layer", "temperature", "flow", "retraction", "retraction_speed", "pressure_advance",
    "max_flow", "speed", "acceleration", "bridges_overhangs", "dimensions", "elephant_foot", "fit")

/** Kinds whose result is read by eye, with no value to apply (the engine's `calibration apply` refuses them). */
val CALIBRATIONS_WITHOUT_VALUE = setOf("dimensions", "first_layer", "fit", "elephant_foot")

fun Context.calibrationName(kind: String): String = getString(when (kind) {
    "first_layer" -> R.string.cal_first_layer; "temperature" -> R.string.cal_temperature; "flow" -> R.string.cal_flow
    "retraction" -> R.string.cal_retraction; "retraction_speed" -> R.string.cal_retraction_speed
    "pressure_advance" -> R.string.cal_pressure_advance; "max_flow" -> R.string.cal_max_flow; "speed" -> R.string.cal_speed
    "acceleration" -> R.string.cal_acceleration; "dimensions" -> R.string.cal_dimensions; "elephant_foot" -> R.string.cal_elephant_foot
    "bridges_overhangs" -> R.string.cal_bridges_overhangs
    else -> R.string.cal_fit
})

/**
 * Calibrate on the phone (calibrations.md): pick a test, optionally its range
 * (start, end, step; empty: the one the settings suggest); the engine makes
 * the part and its settings with the height changes, which go to the plate.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun CalibrateScreen(slice: SliceViewModel, nav: Navigator) {
    val context = ctx()
    var kind by rememberSaveable { mutableStateOf("temperature") }
    var start by rememberSaveable { mutableStateOf("") }
    var end by rememberSaveable { mutableStateOf("") }
    var step by rememberSaveable { mutableStateOf("") }
    var error by remember { mutableStateOf<String?>(null) }
    var busy by remember { mutableStateOf(false) }
    ScreenColumn {
        Text(stringResource(R.string.cal_hint), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            CALIBRATIONS.forEach { k ->
                FilterChip(selected = kind == k, onClick = { kind = k; error = null }, label = { Text(context.calibrationName(k)) },
                    modifier = Modifier.testTag("cal_$k"))
            }
        }
        if (kind !in CALIBRATIONS_WITHOUT_VALUE) {
            Text(stringResource(R.string.cal_range), style = MaterialTheme.typography.labelLarge)
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(start, { start = it }, singleLine = true, label = { Text(stringResource(R.string.cal_start)) },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal), modifier = Modifier.weight(1f).testTag("cal_start"))
                OutlinedTextField(end, { end = it }, singleLine = true, label = { Text(stringResource(R.string.cal_end)) },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal), modifier = Modifier.weight(1f).testTag("cal_end"))
                OutlinedTextField(step, { step = it }, singleLine = true, label = { Text(stringResource(R.string.cal_step)) },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal), modifier = Modifier.weight(1f).testTag("cal_step"))
            }
        }
        error?.let { Text(it, color = MaterialTheme.colorScheme.error, modifier = Modifier.testTag("cal_error")) }
        Button(onClick = {
            val r = listOf(start, end, step).map { it.replace(',', '.').toDoubleOrNull() }
            val range = if (r.all { it != null } && kind !in CALIBRATIONS_WITHOUT_VALUE) r.map { it!! }.toDoubleArray() else null
            busy = true; error = null
            slice.calibrate(kind, range) { e -> busy = false; error = e; if (e == null) nav.select(Section.PLATE) }
        }, enabled = !busy, modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp).testTag("cal_make")) {
            Text(stringResource(R.string.cal_make))
        }
    }
}

/** On the plate, with a calibration test open: its segments, and the value that printed best to apply. */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun CalibrationCard(c: net.tisma.app.engine.Calibrating, slice: SliceViewModel) {
    val context = ctx()
    TismaCard(Modifier.testTag("calibration")) {
        Text(context.calibrationName(c.kind), style = MaterialTheme.typography.titleMedium)
        c.segments.forEach { (z0, z1, v) ->
            Text(stringResource(R.string.cal_segment, fmt(z0), fmt(z1), fmt(v), c.unit), style = MaterialTheme.typography.bodySmall)
        }
        if (c.kind !in CALIBRATIONS_WITHOUT_VALUE && c.segments.isNotEmpty()) {
            Text(stringResource(R.string.cal_best), style = MaterialTheme.typography.labelLarge)
            FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                c.segments.map { it.third }.distinct().forEach { v ->
                    androidx.compose.material3.OutlinedButton(onClick = { slice.applyCalibration(v) },
                        modifier = Modifier.testTag("cal_value_${fmt(v)}")) { Text("${fmt(v)} ${c.unit}") }
                }
            }
        } else Text(stringResource(R.string.cal_by_eye), style = MaterialTheme.typography.bodySmall)
        androidx.compose.material3.TextButton(onClick = { slice.closeCalibration() }) { Text(stringResource(R.string.cal_close)) }
    }
}

private fun fmt(v: Double) = if (v % 1.0 == 0.0) v.toLong().toString() else String.format(java.util.Locale.ROOT, "%.3f", v).trimEnd('0').trimEnd('.')
