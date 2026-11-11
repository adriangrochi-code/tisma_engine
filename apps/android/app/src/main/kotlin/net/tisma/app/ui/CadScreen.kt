package net.tisma.app.ui

import androidx.annotation.StringRes
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.FilterChip
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
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
import net.tisma.app.R
import net.tisma.app.engine.DesignDoc
import net.tisma.app.engine.DesignDoc.Operation
import net.tisma.app.engine.DesignDoc.Primitive
import net.tisma.app.engine.SliceState
import net.tisma.app.engine.SliceViewModel
import java.util.Locale

/**
 * Design (CAD) on the phone (cad_design.md): the history of features, new
 * bodies from the primitives, operations on them, and «Build», which the
 * engine checks and builds through SolidCraft; the bodies show in the 3D view
 * and go to the plate. Edges and faces are not picked yet (fillet, chamfer,
 * hole): those stay on the desktop for now.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun CadScreen(state: SliceState, slice: SliceViewModel, nav: Navigator) {
    if (!state.solidcraft) {
        ScreenColumn { Text(stringResource(R.string.cad_no_solidcraft), color = MaterialTheme.colorScheme.onSurfaceVariant) }
        return
    }
    val doc = state.design
    val context = ctx()
    ScreenColumn {
        state.designMesh?.let { m ->
            Plate3D(m, floatArrayOf(maxOf(100f, m.bounds[3] - m.bounds[0] + 20f), maxOf(100f, m.bounds[4] - m.bounds[1] + 20f)),
                Modifier.fillMaxWidth().height(260.dp))
        }
        state.designVolume?.let { Text(stringResource(R.string.cad_volume, String.format(Locale.getDefault(), "%.1f", it / 1000.0)),
            style = MaterialTheme.typography.bodySmall) }
        state.designMessage?.let { Text(ctx().appMessage(it), color = MaterialTheme.colorScheme.error, modifier = Modifier.testTag("cad_message")) }
        if (state.designBusy) LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = { slice.buildDesign() }, enabled = !state.designBusy && doc.features.isNotEmpty(),
                modifier = Modifier.weight(1f).heightIn(min = 48.dp).testTag("cad_build")) { Text(stringResource(R.string.cad_build)) }
            OutlinedButton(onClick = { slice.designToPlate(); nav.select(Section.PLATE) }, enabled = state.designMesh != null && !state.designBusy,
                modifier = Modifier.weight(1f).heightIn(min = 48.dp).testTag("cad_to_plate")) { Text(stringResource(R.string.cad_to_plate)) }
        }
        Text(stringResource(R.string.cad_add_body), style = MaterialTheme.typography.titleSmall)
        FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            DesignDoc.PRIMITIVES.keys.forEach { k ->
                OutlinedButton(onClick = { slice.setDesign(doc.copy(features = doc.features + DesignDoc.primitive(doc, k, context.getString(primitiveName(k))))) },
                    modifier = Modifier.testTag("cad_add_$k")) { Text(stringResource(primitiveName(k))) }
            }
        }
        if (doc.bodies().isNotEmpty()) {
            Text(stringResource(R.string.cad_add_operation), style = MaterialTheme.typography.titleSmall)
            FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                DesignDoc.OPERATIONS.forEach { k ->
                    OutlinedButton(onClick = {
                        val target = doc.bodies().last()
                        val op = Operation(doc.freshName(context.getString(operationName(k))), k, target,
                            value = if (k == "shell") 2.0 else 2.0, boolean = if (k == "combine") "join" else "cut",
                            tools = if (k == "combine") doc.bodies().dropLast(1).takeLast(1) else emptyList(),
                            count = if (k == "pattern_rectangular") 3 else 6)
                        slice.setDesign(doc.copy(features = doc.features + op))
                    }, modifier = Modifier.testTag("cad_op_$k")) { Text(stringResource(operationName(k))) }
                }
            }
        }
        Text(stringResource(R.string.cad_history), style = MaterialTheme.typography.titleSmall)
        doc.features.forEachIndexed { i, f ->
            FeatureCard(doc, i, f) { nf -> slice.setDesign(doc.copy(features = if (nf == null) doc.features.filterIndexed { j, _ -> j != i }
                else doc.features.mapIndexed { j, o -> if (j == i) nf else o })) }
        }
    }
}

@Composable
private fun FeatureCard(doc: DesignDoc, index: Int, f: DesignDoc.Feature, change: (DesignDoc.Feature?) -> Unit) {
    Card(Modifier.fillMaxWidth().testTag("cad_feature_$index")) {
        Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(f.name, style = MaterialTheme.typography.titleMedium, modifier = Modifier.weight(1f))
                // The eye of the history (rule 3): left out of the build.
                Switch(!f.suppressed, { on ->
                    change(when (f) { is Primitive -> f.copy(suppressed = !on); is Operation -> f.copy(suppressed = !on); else -> f })
                }, modifier = Modifier.named(stringResource(R.string.cad_included)))
                TextButton(onClick = { change(null) }, modifier = Modifier.testTag("cad_delete_$index")) { Text(stringResource(R.string.eng_remove)) }
            }
            when (f) {
                is Primitive -> sizeLabels(f.kind).forEachIndexed { k, l ->
                    Num(l, f.sizes.getOrElse(k) { 0.0 }, "cad_${index}_size$k") { v -> change(f.copy(sizes = f.sizes.toMutableList().also { it[k] = v })) }
                }
                is Operation -> OperationFields(doc, index, f) { change(it) }
            }
        }
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun OperationFields(doc: DesignDoc, index: Int, o: Operation, change: (Operation) -> Unit) {
    val before = doc.features.take(index).filter { !it.suppressed }.map { it.name }
    Text(stringResource(R.string.cad_target), style = MaterialTheme.typography.labelLarge)
    FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        before.forEach { b -> FilterChip(selected = o.target == b, onClick = { change(o.copy(target = b)) }, label = { Text(b) }) }
    }
    when (o.kind) {
        "move" -> {
            listOf("X", "Y", "Z").forEachIndexed { a, n ->
                Num(stringResource(R.string.cad_offset, n), o.offset[a], "cad_${index}_offset$a") { v -> change(o.copy(offset = o.offset.toMutableList().also { it[a] = v })) }
            }
            Num(stringResource(R.string.cad_angle), o.angle, "cad_${index}_angle") { change(o.copy(angle = it)) }
            Axis(o.axis) { change(o.copy(axis = it)) }
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(stringResource(R.string.cad_copy), modifier = Modifier.weight(1f))
                Switch(o.copy, { change(o.copy(copy = it)) }, modifier = Modifier.named(stringResource(R.string.cad_copy)))
            }
        }
        "combine" -> {
            FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                listOf("join" to R.string.cad_join, "cut" to R.string.cad_cut, "intersect" to R.string.cad_intersect).forEach { (b, l) ->
                    FilterChip(selected = o.boolean == b, onClick = { change(o.copy(boolean = b)) }, label = { Text(stringResource(l)) },
                        modifier = Modifier.testTag("cad_${index}_$b"))
                }
            }
            Text(stringResource(R.string.cad_tools), style = MaterialTheme.typography.labelLarge)
            FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                before.filter { it != o.target }.forEach { b ->
                    FilterChip(selected = b in o.tools, onClick = { change(o.copy(tools = if (b in o.tools) o.tools - b else o.tools + b)) },
                        label = { Text(b) }, modifier = Modifier.testTag("cad_${index}_tool_$b"))
                }
            }
        }
        "mirror" -> FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            listOf("XY", "XZ", "YZ").forEach { p -> FilterChip(selected = o.plane == p, onClick = { change(o.copy(plane = p)) }, label = { Text(p) }) }
        }
        "pattern_circular" -> {
            Num(stringResource(R.string.cad_count), o.count.toDouble(), "cad_${index}_count") { change(o.copy(count = it.toInt().coerceAtLeast(2))) }
            Num(stringResource(R.string.cad_angle), o.angle.takeIf { it > 0 } ?: 360.0, "cad_${index}_angle") { change(o.copy(angle = it)) }
            Axis(o.axis) { change(o.copy(axis = it)) }
        }
        "pattern_rectangular" -> {
            Num(stringResource(R.string.cad_count), o.count.toDouble(), "cad_${index}_count") { change(o.copy(count = it.toInt().coerceAtLeast(2))) }
            Num(stringResource(R.string.cad_spacing), o.spacing, "cad_${index}_spacing") { change(o.copy(spacing = it)) }
            Axis(when { o.direction[1] != 0.0 -> "Y"; o.direction[2] != 0.0 -> "Z"; else -> "X" }) { a ->
                change(o.copy(direction = listOf(if (a == "X") 1.0 else 0.0, if (a == "Y") 1.0 else 0.0, if (a == "Z") 1.0 else 0.0)))
            }
        }
        "shell" -> Num(stringResource(R.string.cad_thickness), o.value, "cad_${index}_value") { change(o.copy(value = it)) }
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun Axis(axis: String, set: (String) -> Unit) = FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
    listOf("X", "Y", "Z").forEach { a -> FilterChip(selected = axis == a, onClick = { set(a) }, label = { Text(a) }) }
}

@Composable
private fun Num(label: String, value: Double, tag: String, set: (Double) -> Unit) {
    var text by remember(tag) { mutableStateOf(value.toString().removeSuffix(".0")) }
    val onText: (String) -> Unit = { t -> text = t; t.replace(',', '.').replace('−', '-').toDoubleOrNull()?.let(set) }
    // Offsets and angles may be negative, and the number keyboard has no minus on many phones.
    androidx.compose.foundation.layout.Row(verticalAlignment = androidx.compose.ui.Alignment.CenterVertically) {
        OutlinedTextField(text, onText, singleLine = true, label = { Text(label) },
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal), modifier = Modifier.weight(1f).testTag(tag))
        SignButton(text, tag + "_sign", onText)
    }
}

@StringRes
private fun primitiveName(k: String) = when (k) {
    "box" -> R.string.cad_box; "cylinder" -> R.string.cad_cylinder; "sphere" -> R.string.cad_sphere
    "cone" -> R.string.cad_cone; "tube" -> R.string.cad_tube; else -> R.string.cad_torus
}

@StringRes
private fun operationName(k: String) = when (k) {
    "move" -> R.string.cad_move; "combine" -> R.string.cad_combine; "mirror" -> R.string.cad_mirror
    "pattern_circular" -> R.string.cad_pattern_circular; "pattern_rectangular" -> R.string.cad_pattern_rectangular
    else -> R.string.cad_shell
}

/** The sizes of each primitive, in the engine's order (src/cad/design.hpp). */
@Composable
private fun sizeLabels(kind: String): List<String> = when (kind) {
    "box" -> listOf(R.string.cad_length, R.string.cad_width, R.string.cad_height)
    "cylinder" -> listOf(R.string.cad_diameter, R.string.cad_height)
    "sphere" -> listOf(R.string.cad_diameter)
    "cone" -> listOf(R.string.cad_base_diameter, R.string.cad_top_diameter, R.string.cad_height)
    "tube" -> listOf(R.string.cad_outer_diameter, R.string.cad_inner_diameter, R.string.cad_height)
    else -> listOf(R.string.cad_ring_diameter, R.string.cad_section_diameter)
}.map { stringResource(it) }
