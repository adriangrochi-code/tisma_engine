package net.tisma.app.ui

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.size
import androidx.compose.material3.Button
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import net.tisma.app.R
import net.tisma.app.engine.SliceNotice
import net.tisma.app.engine.SliceState
import net.tisma.app.engine.SliceViewModel
import net.tisma.app.printers.PrintersState
import net.tisma.app.printers.PrintersViewModel
import net.tisma.printers.PrinterState
import java.util.Locale

/**
 * The plate: open a model, pick a quality and supports, slice it on the phone
 * with the engine, then save the G-code or send it to a printer (mobile_app.md,
 * rules 6 and 12; MOBILE.md, step 5). The cloud stays one tap away.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun PlateScreen(
    state: SliceState,
    slice: SliceViewModel,
    printerState: PrintersState,
    printers: PrintersViewModel,
    signedIn: Boolean,
    nav: Navigator,
) {
    val context = ctx()
    var sendTo by rememberSaveable { mutableStateOf<String?>(null) }
    val pick = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        if (uri != null) slice.open(displayName(context, uri) ?: "model.stl") { context.contentResolver.openInputStream(uri) }
    }
    val save = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("text/x.gcode")) { uri: Uri? ->
        if (uri != null) slice.saveGcode(displayName(context, uri) ?: "print.gcode") {
            context.contentResolver.openOutputStream(uri, "wt")
        }
    }
    // Tablets and landscape (mobile_app.md, rule 5; accessibility_touch.md, rule 11): with room, the 3D view
    // takes the left and the controls the right, so neither scrolls the other away.
    val wide = androidx.compose.ui.platform.LocalConfiguration.current.screenWidthDp >= 720
    // Engineering's marked faces: fixed in sky blue, loads in orange (Okabe and Ito, readable with color blindness).
    val marks = androidx.compose.runtime.remember(state.case.conditions, state.mesh) {
        val mesh = state.mesh
        if (mesh == null) emptyList() else state.case.conditions.map { c ->
            val f = FloatArray(9 * c.triangles.size)
            c.triangles.forEachIndexed { j, t -> System.arraycopy(mesh.positions, 9 * t, f, 9 * j, 9) }
            f to (if (c.kind == net.tisma.app.engine.EngineeringCase.Kind.FIXED) 0x56B4E9 else 0xE69F00)
        }
    }
    val view: @Composable (Modifier) -> Unit = { m ->
        Plate3D(state.mesh, state.bed, m.clip(androidx.compose.foundation.shape.RoundedCornerShape(12.dp)),
            marks = marks, onPick = if (state.marking != null) slice::pick else null, lockRotation = state.marking != null)
    }
    if (wide && state.modelName != null) {
        Row(Modifier.fillMaxSize()) {
            view(Modifier.weight(1.3f).fillMaxHeight().padding(start = 16.dp, top = 16.dp, bottom = 16.dp))
            Box(Modifier.weight(1f)) { PlateControls(state, slice, printerState, printers, signedIn, nav, sendTo, { sendTo = it }, pick, save, null) }
        }
    } else {
        PlateControls(state, slice, printerState, printers, signedIn, nav, sendTo, { sendTo = it }, pick, save,
            if (state.modelName != null) view else null)
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun PlateControls(
    state: SliceState,
    slice: SliceViewModel,
    printerState: PrintersState,
    printers: PrintersViewModel,
    signedIn: Boolean,
    nav: Navigator,
    sendTo: String?,
    setSendTo: (String?) -> Unit,
    pick: androidx.activity.compose.ManagedActivityResultLauncher<Array<String>, Uri?>,
    save: androidx.activity.compose.ManagedActivityResultLauncher<String, Uri?>,
    view: (@Composable (Modifier) -> Unit)?,
) {
    val context = ctx()
    ScreenColumn {
        PrinterCard(state) { nav.open(Screen.ProfilePicker) }
        view?.invoke(Modifier.fillMaxWidth().height(320.dp))
        if (!state.engineAvailable) {
            Text(stringResource(R.string.plate_no_engine), color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        TismaCard {
            OutlinedButton(onClick = { pick.launch(arrayOf("*/*")) }, enabled = !state.slicing,
                modifier = Modifier.fillMaxWidth().height(52.dp).testTag("plate_open")) {
                Icon(painterResource(R.drawable.tisma_folder), null)
                Spacer(Modifier.size(8.dp))
                Text(stringResource(R.string.plate_open))
            }
            Text(state.modelName ?: stringResource(R.string.plate_no_model), style = MaterialTheme.typography.bodyLarge,
                modifier = Modifier.testTag("plate_model"))
            if (state.modelName != null && state.isProject) {
                Text(stringResource(R.string.plate_project_settings), style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            if (state.modelName != null && !state.isProject) {
                Text(stringResource(R.string.plate_quality), style = MaterialTheme.typography.labelLarge)
                FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    listOf(0.12 to R.string.plate_quality_fine, 0.20 to R.string.plate_quality_normal,
                        0.28 to R.string.plate_quality_draft).forEach { (h, label) ->
                        FilterChip(selected = state.settings.layerHeight == h, enabled = !state.slicing,
                            onClick = { slice.setSettings(state.settings.copy(layerHeight = h)) },
                            label = { Text(stringResource(label, String.format(Locale.getDefault(), "%.2f", h))) })
                    }
                }
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(stringResource(R.string.plate_supports), modifier = Modifier.weight(1f))
                    Switch(state.settings.supports, { slice.setSettings(state.settings.copy(supports = it)) },
                        enabled = !state.slicing, modifier = Modifier.named(stringResource(R.string.plate_supports)))
                }
            }
        }
        if (state.slicing) {
            TismaCard {
                Text(stringResource(R.string.plate_slicing, state.progress), modifier = Modifier.testTag("plate_progress"))
                LinearProgressIndicator(progress = { state.progress / 100f }, modifier = Modifier.fillMaxWidth())
                OutlinedButton(onClick = { slice.cancel() }, modifier = Modifier.fillMaxWidth().testTag("plate_cancel")) {
                    Text(stringResource(R.string.cancel))
                }
            }
        } else {
            Button(onClick = { slice.slice() }, enabled = state.engineAvailable && state.modelName != null,
                modifier = Modifier.fillMaxWidth().height(56.dp).testTag("plate_slice")) {
                Icon(painterResource(R.drawable.tisma_slice), null)
                Spacer(Modifier.size(8.dp))
                Text(stringResource(R.string.plate_slice))
            }
        }
        state.tooBig?.let { (need, free) ->
            // MA9: too big for this phone: the cloud is offered (rule 12).
            TismaCard(Modifier.testTag("too_big")) {
                Text(stringResource(R.string.mem_too_big, need shr 20, free shr 20), color = MaterialTheme.colorScheme.error)
                Button(onClick = { slice.dismissTooBig(); nav.open(if (signedIn) Screen.NewJob else Screen.Account) },
                    modifier = Modifier.fillMaxWidth().testTag("too_big_cloud")) { Text(stringResource(R.string.plate_slice_cloud)) }
                AccentTextButton(onClick = { slice.sliceAnyway() }) { Text(stringResource(R.string.mem_try_anyway)) }
            }
        }
        if (state.modelName != null && state.isProject && state.engineAvailable) EngineeringCard(state, slice)
        if (state.modelName != null && !state.isProject && state.engineAvailable && state.mesh != null) ModelEngineeringCard(state, slice)
        state.error?.let { e ->
            TismaCard {
                Text(stringResource(R.string.job_error), style = MaterialTheme.typography.titleMedium)
                val text = if (context.isSpanish() && e.errorEs.isNotBlank()) e.errorEs
                    else context.jobCodeText(e.code) ?: e.error
                Text(text, color = MaterialTheme.colorScheme.error, modifier = Modifier.testTag("plate_error"))
            }
        }
        state.result?.let { r ->
            TismaCard(Modifier.testTag("plate_result")) {
                Text(stringResource(R.string.plate_result), style = MaterialTheme.typography.titleMedium)
                if (!r.profileUsed) Text(stringResource(R.string.profile_not_used), style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.error, modifier = Modifier.testTag("profile_not_used"))
                if (r.printTime.isNotBlank()) Labeled(R.string.plate_time, r.printTime)
                r.filamentMm?.let { mm ->
                    Labeled(R.string.plate_filament, String.format(Locale.getDefault(), "%.2f m", mm / 1000) +
                        (r.filamentG?.let { g -> String.format(Locale.getDefault(), " · %.1f g", g) } ?: ""))
                }
                r.layers?.let { Labeled(R.string.plate_layers, it.toString()) }
                OutlinedButton(onClick = { nav.open(Screen.Preview(r.gcode.path)) },
                    modifier = Modifier.fillMaxWidth().height(52.dp).testTag("plate_preview")) {
                    Text(stringResource(R.string.preview_open))
                }
                Button(onClick = { save.launch(r.gcode.name) }, modifier = Modifier.fillMaxWidth().height(52.dp)) {
                    Icon(painterResource(R.drawable.tisma_save), null)
                    Spacer(Modifier.size(8.dp))
                    Text(stringResource(R.string.plate_save))
                }
                val ready = printerState.printers.filter {
                    printerState.status[it.id]?.state.let { s -> s == null || s == PrinterState.READY }
                }
                if (ready.isNotEmpty()) {
                    Text(stringResource(R.string.plate_send_to), style = MaterialTheme.typography.labelLarge)
                    FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        ready.forEach { p ->
                            FilterChip(selected = sendTo == p.id, onClick = { setSendTo(p.id) }, label = { Text(p.name) })
                        }
                    }
                    Button(
                        onClick = {
                            val id = sendTo ?: return@Button
                            printers.upload(id, r.gcode.name, r.gcode.length(), true) { r.gcode.inputStream() }
                        },
                        enabled = sendTo != null && !printerState.uploading,
                        modifier = Modifier.fillMaxWidth().height(52.dp).testTag("plate_send"),
                    ) {
                        Icon(painterResource(R.drawable.tisma_send), null)
                        Spacer(Modifier.size(8.dp))
                        Text(stringResource(R.string.plate_print))
                    }
                }
            }
        }
        // The cloud stays one tap away (mobile_app.md, rules 12 and 15).
        AccentTextButton(onClick = { nav.open(if (signedIn) Screen.NewJob else Screen.Account) },
            modifier = Modifier.testTag("plate_slice_cloud")) {
            Text(stringResource(R.string.plate_slice_cloud))
        }
    }
}

/**
 * Engineering on the phone (mobile_app.md, rule 13): whether the part holds, with the
 * supports, loads and material saved in the project, and the air's drag; both with the
 * engine, like the desktop and the cloud. Picking faces needs the 3D view: the
 * conditions come from the project for now.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun EngineeringCard(state: SliceState, slice: SliceViewModel) {
    val context = ctx()
    var objectIndex by rememberSaveable { mutableStateOf("0") }
    var quality by rememberSaveable { mutableStateOf("normal") }
    TismaCard(Modifier.testTag("engineering")) {
        Text(stringResource(R.string.eng_title), style = MaterialTheme.typography.titleMedium)
        Text(stringResource(R.string.eng_hint), style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant)
        androidx.compose.material3.OutlinedTextField(objectIndex, { v -> objectIndex = v.filter(Char::isDigit).take(3) },
            singleLine = true, label = { Text(stringResource(R.string.object_index)) },
            keyboardOptions = androidx.compose.foundation.text.KeyboardOptions(keyboardType = androidx.compose.ui.text.input.KeyboardType.Number),
            modifier = Modifier.fillMaxWidth())
        FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            listOf("fast" to R.string.quality_fast, "normal" to R.string.quality_normal, "high" to R.string.quality_high).forEach { (q, l) ->
                FilterChip(selected = quality == q, onClick = { quality = q }, label = { Text(stringResource(l)) })
            }
        }
        val busy = state.analyzing || state.slicing
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = { slice.analyze(SliceViewModel.Study.STRUCTURE, objectIndex.toIntOrNull() ?: 0, quality) },
                enabled = !busy, modifier = Modifier.weight(1f).testTag("eng_analyze")) {
                Text(stringResource(R.string.eng_analyze))
            }
            OutlinedButton(onClick = { slice.analyze(SliceViewModel.Study.AERO, objectIndex.toIntOrNull() ?: 0,
                    if (quality == "high") "normal" else "fast") },
                enabled = !busy, modifier = Modifier.weight(1f).testTag("eng_aero")) {
                Text(stringResource(R.string.kind_aero))
            }
        }
        OutlinedButton(onClick = { slice.analyze(SliceViewModel.Study.LIGHTEST, objectIndex.toIntOrNull() ?: 0, quality) },
            enabled = !busy, modifier = Modifier.fillMaxWidth().testTag("eng_lightest")) {
            Text(stringResource(R.string.eng_lightest))
        }
        if (state.analyzing) {
            LinearProgressIndicator(progress = { state.progress / 100f }, modifier = Modifier.fillMaxWidth())
            OutlinedButton(onClick = { slice.cancel() }, modifier = Modifier.fillMaxWidth()) { Text(stringResource(R.string.cancel)) }
        }
        state.analysis?.let { lines ->
            context.readableSummary(lines).forEach { (k, v) ->
                Row { Text(k, Modifier.weight(1f), color = MaterialTheme.colorScheme.onSurfaceVariant); Text(v) }
            }
        }
        // Rule 13: «Relleno mínimo» offers to apply what it found to the slice.
        state.lightest?.let { d ->
            val applied = (state.settings.changes["infill.density"] as? kotlinx.serialization.json.JsonPrimitive)?.content?.toDoubleOrNull()?.toInt() == d
            Button(onClick = { slice.setValue("infill.density", kotlinx.serialization.json.JsonPrimitive(d)) }, enabled = !applied,
                modifier = Modifier.fillMaxWidth().testTag("eng_apply_lightest")) {
                Text(stringResource(if (applied) R.string.eng_lightest_applied else R.string.eng_lightest_apply, d))
            }
        }
    }
}

/**
 * Engineering on a bare model (mobile_app.md, rule 13): pick what a tap
 * marks, touch the faces, set each load's force, the material and the safety
 * asked, then Analyze. The verdict compares the engine's lowest safety factor
 * with the one asked.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun ModelEngineeringCard(state: SliceState, slice: SliceViewModel) {
    val context = ctx()
    val case = state.case
    TismaCard(Modifier.testTag("model_engineering")) {
        Text(stringResource(R.string.eng_title), style = MaterialTheme.typography.titleMedium)
        Text(stringResource(R.string.eng_model_hint), style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant)
        FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            listOf(null to R.string.eng_mark_off, net.tisma.app.engine.EngineeringCase.Kind.FIXED to R.string.eng_mark_fixed,
                net.tisma.app.engine.EngineeringCase.Kind.LOAD to R.string.eng_mark_load).forEach { (k, l) ->
                FilterChip(selected = state.marking == k, onClick = { slice.setMarking(k) }, label = { Text(stringResource(l)) },
                    modifier = Modifier.testTag("eng_mark_" + (k?.name?.lowercase() ?: "off")))
            }
        }
        var fixedN = 0; var loadN = 0
        case.conditions.forEachIndexed { i, c ->
            val fixed = c.kind == net.tisma.app.engine.EngineeringCase.Kind.FIXED
            Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.testTag("eng_condition_$i")) {
                Text(if (fixed) stringResource(R.string.eng_fixed, ++fixedN) else stringResource(R.string.eng_load, ++loadN),
                    modifier = Modifier.weight(1f))
                if (!fixed) {
                    var text by rememberSaveable(i, c.newtons) { mutableStateOf(c.newtons.toString().removeSuffix(".0")) }
                    androidx.compose.material3.OutlinedTextField(text, { t ->
                        text = t
                        t.replace(',', '.').toDoubleOrNull()?.takeIf { it > 0 }?.let { n ->
                            slice.setCase(case.copy(conditions = case.conditions.mapIndexed { j, x -> if (j == i) x.copy(newtons = n) else x }))
                        }
                    }, singleLine = true, label = { Text(stringResource(R.string.eng_newtons)) },
                        keyboardOptions = androidx.compose.foundation.text.KeyboardOptions(keyboardType = androidx.compose.ui.text.input.KeyboardType.Decimal),
                        modifier = Modifier.weight(1f).testTag("eng_newtons_$i"))
                }
                androidx.compose.material3.TextButton(onClick = { slice.removeCondition(i) }) { Text(stringResource(R.string.eng_remove)) }
            }
        }
        // The material (docs/MATERIALS.md) and the safety factor the part must keep.
        var open by androidx.compose.runtime.remember { mutableStateOf(false) }
        androidx.compose.foundation.layout.Box {
            OutlinedButton(onClick = { open = true }, modifier = Modifier.testTag("eng_material")) {
                Text(stringResource(R.string.eng_material) + ": " + case.material)
            }
            androidx.compose.material3.DropdownMenu(open, { open = false }) {
                net.tisma.app.engine.EngineeringCase.MATERIALS.forEach { m ->
                    androidx.compose.material3.DropdownMenuItem(text = { Text(m) }, onClick = { open = false; slice.setCase(case.copy(material = m)) })
                }
            }
        }
        var safety by rememberSaveable { mutableStateOf(case.requiredSafety.toString().removeSuffix(".0")) }
        androidx.compose.material3.OutlinedTextField(safety, { t ->
            safety = t
            t.replace(',', '.').toDoubleOrNull()?.takeIf { it >= 1 }?.let { slice.setCase(case.copy(requiredSafety = it)) }
        }, singleLine = true, label = { Text(stringResource(R.string.eng_required)) },
            keyboardOptions = androidx.compose.foundation.text.KeyboardOptions(keyboardType = androidx.compose.ui.text.input.KeyboardType.Decimal),
            modifier = Modifier.fillMaxWidth().testTag("eng_required"))
        FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            listOf("normal" to R.string.quality_normal, "high" to R.string.quality_high).forEach { (q, l) ->
                FilterChip(selected = case.quality == q, onClick = { slice.setCase(case.copy(quality = q)) }, label = { Text(stringResource(l)) })
            }
        }
        val ready = case.conditions.any { it.kind == net.tisma.app.engine.EngineeringCase.Kind.FIXED } &&
            case.conditions.any { it.kind == net.tisma.app.engine.EngineeringCase.Kind.LOAD }
        if (!ready) Text(stringResource(R.string.eng_need_both), style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant)
        Button(onClick = { slice.analyzeModel() }, enabled = ready && !state.analyzing && !state.slicing,
            modifier = Modifier.fillMaxWidth().testTag("eng_analyze_model")) { Text(stringResource(R.string.eng_analyze)) }
        if (state.analyzing) {
            LinearProgressIndicator(progress = { state.progress / 100f }, modifier = Modifier.fillMaxWidth())
            OutlinedButton(onClick = { slice.cancel() }, modifier = Modifier.fillMaxWidth()) { Text(stringResource(R.string.cancel)) }
        }
        state.analysis?.let { lines ->
            context.readableSummary(lines).forEach { (k, v) ->
                Row { Text(k, Modifier.weight(1f), color = MaterialTheme.colorScheme.onSurfaceVariant); Text(v) }
            }
        }
    }
}

fun Context.sliceNoticeText(n: SliceNotice): String = when (n) {
    SliceNotice.Cancelled -> getString(R.string.plate_cancelled)
    SliceNotice.CannotRead -> getString(R.string.error_cannot_read)
    is SliceNotice.Saved -> getString(R.string.notice_saved, n.name)
}

private fun displayName(context: Context, uri: Uri): String? = runCatching {
    context.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { c ->
        if (c.moveToFirst()) c.getString(0) else null
    }
}.getOrNull()
