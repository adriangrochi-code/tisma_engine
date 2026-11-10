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
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
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
    /** The Engineering section (owner's decision, 05-11-2026): the same part, with FEA, CFD, CAD and parts. */
    engineering: Boolean = false,
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
            marks = marks, onPick = if (state.marking != null || state.layFlat) slice::pick else null,
            lockRotation = state.marking != null || state.layFlat,
            bedModel = state.bedModel, bedTexture = state.bedTexture, stress = state.stressMap?.takeIf { state.showStress })
    }
    if (wide && state.modelName != null) {
        Row(Modifier.fillMaxSize()) {
            view(Modifier.weight(1.3f).fillMaxHeight().padding(start = 16.dp, top = 16.dp, bottom = 16.dp))
            Box(Modifier.weight(1f)) { PlateControls(state, slice, printerState, printers, signedIn, nav, sendTo, { sendTo = it }, pick, save, null, engineering) }
        }
    } else {
        PlateControls(state, slice, printerState, printers, signedIn, nav, sendTo, { sendTo = it }, pick, save,
            if (state.modelName != null) view else null, engineering)
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
    engineering: Boolean = false,
) {
    val context = ctx()
    if (engineering) { EngineeringControls(state, slice, nav, pick, view); return }
    ScreenColumn {
        PrinterCard(state) { nav.open(Screen.ProfilePicker) }
        view?.invoke(Modifier.fillMaxWidth().height(320.dp))
        if (!state.engineAvailable) {
            Text(stringResource(R.string.plate_no_engine), color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        TismaCard {
            OutlinedButton(onClick = { pick.launch(arrayOf("*/*")) }, enabled = !state.slicing,
                modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp).testTag("plate_open")) {
                Icon(painterResource(R.drawable.tisma_folder), null)
                Spacer(Modifier.size(8.dp))
                Text(stringResource(R.string.plate_open))
            }
            Text(state.modelName ?: stringResource(R.string.plate_no_model), style = MaterialTheme.typography.bodyLarge,
                modifier = Modifier.testTag("plate_model"))
            // Rule 6, «Archivo»: the part's size and triangles, and taking it off the plate.
            state.mesh?.takeIf { state.modelName != null }?.let { m ->
                val b = m.bounds
                Text(stringResource(R.string.plate_info, String.format(Locale.getDefault(), "%.1f × %.1f × %.1f", b[3] - b[0], b[4] - b[1], b[5] - b[2]),
                    m.triangles), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.testTag("plate_info"))
            }
            if (state.modelName != null) AccentTextButton(onClick = { slice.removeModel() }, enabled = !state.slicing && !state.analyzing,
                modifier = Modifier.testTag("plate_remove")) { Text(stringResource(R.string.plate_remove)) }
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
                // gcode_macros.md, rule 12: a Bambu Lab's plate goes into its start G-code (curr_bed_type), so it is a
                // slicing setting; the other options are chosen when sending.
                if (isBambu(state.profile)) {
                    Text(stringResource(R.string.bambu_plate), style = MaterialTheme.typography.labelLarge)
                    val plate = (state.value("bambu.plate") as? kotlinx.serialization.json.JsonPrimitive)?.content.orEmpty()
                    FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        (listOf("") + BAMBU_PLATES).forEach { name ->
                            FilterChip(selected = plate == name, enabled = !state.slicing,
                                onClick = { slice.setValue("bambu.plate", if (name.isEmpty()) null else kotlinx.serialization.json.JsonPrimitive(name)) },
                                label = { Text(name.ifEmpty { stringResource(R.string.bambu_plate_auto) }) },
                                modifier = Modifier.testTag("bambu_plate_" + name.ifEmpty { "auto" }))
                        }
                    }
                }
                // Copies of the model, which the engine arranges on the bed.
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(stringResource(R.string.plate_copies), modifier = Modifier.weight(1f))
                    OutlinedButton(onClick = { slice.setSettings(state.settings.copy(copies = (state.settings.copies - 1).coerceAtLeast(1))) },
                        enabled = !state.slicing && state.settings.copies > 1, modifier = Modifier.testTag("copies_less")) { Text("−") }
                    Text(state.settings.copies.toString(), modifier = Modifier.padding(horizontal = 12.dp).testTag("copies"))
                    OutlinedButton(onClick = { slice.setSettings(state.settings.copy(copies = (state.settings.copies + 1).coerceAtMost(50))) },
                        enabled = !state.slicing && state.settings.copies < 50, modifier = Modifier.testTag("copies_more")) { Text("+") }
                }
            }
        }
        // The Print settings open from here; Material is in the Printer section (owner's decision, 05-11-2026).
        OutlinedButton(onClick = { nav.open(Screen.SettingsTab("print")) }, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)
            .testTag("plate_print_settings")) { Text(stringResource(R.string.section_print)) }
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
                modifier = Modifier.fillMaxWidth().heightIn(min = 56.dp).testTag("plate_slice")) {
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
        state.calibration?.let { CalibrationCard(it, slice) }
        if (state.modelName != null && !state.isProject && state.mesh != null) TransformCard(state, slice)
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
                // mobile_app.md, rule 12: the cost, from the filament's price per kg (Material, extrusion.cost).
                plateCost(r.filamentG, state.value("extrusion.cost"))?.let { c ->
                    Labeled(R.string.plate_cost, String.format(Locale.getDefault(), "%.2f", c), Modifier.testTag("plate_cost"))
                }
                OutlinedButton(onClick = { nav.open(Screen.Preview(r.gcode.path)) },
                    modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp).testTag("plate_preview")) {
                    Text(stringResource(R.string.preview_open))
                }
                Button(onClick = { save.launch(r.gcode.name) }, modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp)) {
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
                            FilterChip(selected = sendTo == p.id, onClick = { setSendTo(p.id) }, label = { Text(p.name) }, modifier = Modifier.testTag("send_to_" + p.name))
                        }
                    }
                    // gcode_macros.md, rule 12: what a Bambu Lab does before printing, chosen when sending.
                    var leveling by rememberSaveable { mutableStateOf(true) }
                    var flow by rememberSaveable { mutableStateOf(true) }
                    var vibration by rememberSaveable { mutableStateOf(false) }
                    var timelapse by rememberSaveable { mutableStateOf(false) }
                    val bambu = ready.firstOrNull { it.id == sendTo }?.kind == net.tisma.printers.PrinterKind.BAMBU
                    if (bambu) {
                        Text(stringResource(R.string.bambu_options), style = MaterialTheme.typography.labelLarge)
                        listOf(Triple("leveling", R.string.bambu_leveling, leveling to { v: Boolean -> leveling = v }),
                            Triple("flow", R.string.bambu_flow, flow to { v: Boolean -> flow = v }),
                            Triple("vibration", R.string.bambu_vibration, vibration to { v: Boolean -> vibration = v }),
                            Triple("timelapse", R.string.bambu_timelapse, timelapse to { v: Boolean -> timelapse = v })).forEach { (tag, label, value) ->
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Text(stringResource(label), modifier = Modifier.weight(1f))
                                Switch(value.first, value.second, modifier = Modifier.testTag("bambu_option_$tag").named(stringResource(label)))
                            }
                        }
                    }
                    Button(
                        onClick = {
                            val id = sendTo ?: return@Button
                            printers.upload(id, r.gcode.name, r.gcode.length(), true,
                                net.tisma.printers.PrintOptions(leveling, flow, vibration, timelapse)) { r.gcode.inputStream() }
                        },
                        enabled = sendTo != null && !printerState.uploading,
                        modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp).testTag("plate_send"),
                    ) {
                        Icon(painterResource(R.drawable.tisma_send), null)
                        Spacer(Modifier.size(8.dp))
                        Text(stringResource(R.string.plate_print))
                    }
                }
            }
        }
        // A 3MF project too (QA, 06-11-2026): the engine keeps it as the entry's project.3mf as it is.
        if (state.libraryAvailable && state.modelName != null &&
            (state.result != null || state.analysis != null || state.fromLibrary != null)) LibraryCard(state, slice)
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
        if (state.analyzing) AnalysisProgress(state, slice)
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
            FilterChip(selected = case.solid, onClick = { slice.setCase(case.copy(solid = true, quality = "normal")) },
                label = { Text(stringResource(R.string.eng_fast_solid)) }, modifier = Modifier.testTag("eng_quality_fast"))
            listOf("normal" to R.string.quality_normal, "high" to R.string.quality_high).forEach { (q, l) ->
                FilterChip(selected = !case.solid && case.quality == q, onClick = { slice.setCase(case.copy(quality = q, solid = false)) },
                    label = { Text(stringResource(l)) })
            }
        }
        val ready = case.conditions.any { it.kind == net.tisma.app.engine.EngineeringCase.Kind.FIXED } &&
            case.conditions.any { it.kind == net.tisma.app.engine.EngineeringCase.Kind.LOAD }
        if (!ready) Text(stringResource(R.string.eng_need_both), style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant)
        Button(onClick = { slice.analyzeModel() }, enabled = ready && !state.analyzing && !state.slicing,
            modifier = Modifier.fillMaxWidth().testTag("eng_analyze_model")) { Text(stringResource(R.string.eng_analyze)) }
        state.slowAnalysis?.let { secs ->
            Text(stringResource(R.string.eng_slow, Math.round(secs / 60.0).coerceAtLeast(1).toInt()), modifier = Modifier.testTag("eng_slow"))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(onClick = { slice.analyzeModel(force = true) }, modifier = Modifier.testTag("eng_run_anyway")) { Text(stringResource(R.string.eng_run_anyway)) }
                if (!case.solid) OutlinedButton(onClick = { slice.setCase(case.copy(solid = true)); slice.analyzeModel() },
                    modifier = Modifier.testTag("eng_go_fast")) { Text(stringResource(R.string.eng_go_fast)) }
            }
        }
        // aerodynamics.md on the bare model: the drag at a speed, along an axis.
        var speed by rememberSaveable { mutableStateOf("10") }
        var dir by rememberSaveable { mutableStateOf("x") }
        Text(stringResource(R.string.kind_aero), style = MaterialTheme.typography.labelLarge)
        FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            listOf("x", "-x", "y", "-y", "z", "-z").forEach { d ->
                FilterChip(selected = dir == d, onClick = { dir = d }, label = { Text(d) }, modifier = Modifier.testTag("aero_dir_$d"))
            }
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            androidx.compose.material3.OutlinedTextField(speed, { speed = it }, singleLine = true, label = { Text(stringResource(R.string.aero_speed)) },
                keyboardOptions = androidx.compose.foundation.text.KeyboardOptions(keyboardType = androidx.compose.ui.text.input.KeyboardType.Decimal),
                modifier = Modifier.weight(1f).testTag("aero_speed"))
            Spacer(Modifier.size(8.dp))
            OutlinedButton(onClick = {
                speed.replace(',', '.').toDoubleOrNull()?.takeIf { it > 0 }?.let {
                    slice.analyze(SliceViewModel.Study.AERO, 0, if (case.quality == "high") "normal" else "fast", it, dir)
                }
            }, enabled = !state.analyzing && !state.slicing, modifier = Modifier.testTag("aero_model")) { Text(stringResource(R.string.aero_run)) }
        }
        if (state.analyzing) AnalysisProgress(state, slice)
        state.analysis?.let { lines ->
            context.readableSummary(lines).forEach { (k, v) ->
                Row { Text(k, Modifier.weight(1f), color = MaterialTheme.colorScheme.onSurfaceVariant); Text(v) }
            }
        }
        // Rule 13: the map of colors on the part, with its legend (docs/ANALYSIS.md's colors and limits).
        if (state.stressMap != null) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(stringResource(R.string.eng_show_map), modifier = Modifier.weight(1f))
                androidx.compose.material3.Switch(state.showStress, { slice.showStress(it) },
                    modifier = Modifier.testTag("eng_show_map").named(stringResource(R.string.eng_show_map)))
            }
            FlowRow(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                listOf(0xFFDC2828 to R.string.map_fails, 0xFFF58C1E to R.string.map_1_15, 0xFFF0D228 to R.string.map_15_2,
                    0xFF3CAA4B to R.string.map_2, 0xFFBEBEBE to R.string.map_none).forEach { (c, l) ->
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        androidx.compose.foundation.layout.Box(Modifier.size(10.dp).background(androidx.compose.ui.graphics.Color(c)))
                        Spacer(Modifier.size(4.dp))
                        Text(stringResource(l), style = MaterialTheme.typography.bodySmall)
                    }
                }
            }
        }
    }
}

/**
 * The plate's tools on a bare model (the desktop's plate tools; accessibility_touch.md, rule 14): quarter
 * turns about X, Y and Z, mirror, scale in %, and «lay on a face» (the next tap picks the face that goes
 * down). The engine slices the turned model.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun TransformCard(state: SliceState, slice: SliceViewModel) {
    TismaCard(Modifier.testTag("transform")) {
        Text(stringResource(R.string.tools_title), style = MaterialTheme.typography.titleMedium)
        val b = state.mesh?.bounds
        if (b != null) Text(String.format(Locale.getDefault(), "%.1f × %.1f × %.1f mm", b[3] - b[0], b[4] - b[1], b[5] - b[2]),
            style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.testTag("model_size"))
        FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            listOf("X", "Y", "Z").forEachIndexed { axis, name ->
                OutlinedButton(onClick = { slice.rotate(axis) }, enabled = !state.slicing, modifier = Modifier.testTag("rotate_$name")) {
                    Text(stringResource(R.string.tools_rotate, name))
                }
            }
            OutlinedButton(onClick = { slice.mirror(0) }, enabled = !state.slicing, modifier = Modifier.testTag("mirror_x")) {
                Text(stringResource(R.string.tools_mirror))
            }
            FilterChip(selected = state.layFlat, onClick = { slice.setLayFlat(!state.layFlat) }, enabled = !state.slicing,
                label = { Text(stringResource(R.string.tools_lay_flat)) }, modifier = Modifier.testTag("lay_flat"))
            // orientation.md: the engine's least-support orientation.
            if (state.engineAvailable) OutlinedButton(onClick = { slice.orientAutomatically() }, enabled = !state.slicing && !state.analyzing,
                modifier = Modifier.testTag("orient_auto")) { Text(stringResource(R.string.tools_orient)) }
        }
        if (state.layFlat) Text(stringResource(R.string.tools_lay_flat_hint), style = MaterialTheme.typography.bodySmall)
        Row(verticalAlignment = Alignment.CenterVertically) {
            var text by rememberSaveable(state.scalePercent) { mutableStateOf(state.scalePercent.toString().removeSuffix(".0")) }
            androidx.compose.material3.OutlinedTextField(text, { t ->
                text = t
                t.replace(',', '.').toFloatOrNull()?.takeIf { it in 1f..10_000f }?.let { if (it != state.scalePercent) slice.setScale(it) }
            }, singleLine = true, label = { Text(stringResource(R.string.tools_scale)) }, suffix = { Text("%") },
                keyboardOptions = androidx.compose.foundation.text.KeyboardOptions(keyboardType = androidx.compose.ui.text.input.KeyboardType.Decimal),
                modifier = Modifier.weight(1f).testTag("scale"))
            androidx.compose.material3.TextButton(onClick = { slice.resetTransform() }, modifier = Modifier.testTag("transform_reset")) {
                Text(stringResource(R.string.panel_reset))
            }
        }
    }
}

/** An analysis running: the engine's stage and percent over the bar (QA: only «Cancel» showed), and Cancel. */
@Composable
private fun AnalysisProgress(state: SliceState, slice: SliceViewModel) {
    val stage = when (state.progressStage) {
        "estimate" -> R.string.stage_estimate
        "slicing" -> R.string.stage_slicing
        "voxels" -> R.string.stage_voxels
        "solving" -> R.string.stage_solving
        "summary" -> R.string.stage_summary
        "resolution" -> R.string.stage_resolution
        else -> R.string.stage_working
    }
    Text(stringResource(R.string.stage_line, stringResource(stage), state.progress), style = MaterialTheme.typography.bodySmall,
        modifier = Modifier.testTag("analysis_stage"))
    LinearProgressIndicator(progress = { state.progress / 100f }, modifier = Modifier.fillMaxWidth())
    OutlinedButton(onClick = { slice.cancel() }, modifier = Modifier.fillMaxWidth()) { Text(stringResource(R.string.cancel)) }
}

fun Context.sliceNoticeText(n: SliceNotice): String = when (n) {
    SliceNotice.Cancelled -> getString(R.string.plate_cancelled)
    SliceNotice.CannotRead -> getString(R.string.error_cannot_read)
    is SliceNotice.Saved -> getString(R.string.notice_saved, n.name)
    SliceNotice.ProfileGone -> getString(R.string.pack_profile_gone)
    is SliceNotice.PackUpdated -> getString(R.string.pack_updated, n.version)
    is SliceNotice.Calibrated -> getString(R.string.cal_applied, calibrationName(n.kind), n.value.toString().removeSuffix(".0"))
    is SliceNotice.LibrarySaved -> getString(R.string.library_saved, n.name)
    is SliceNotice.LibraryFailed -> getString(R.string.library_failed, n.message)
    is SliceNotice.Unsafe -> getString(R.string.unsafe_file, getString(when (n.problem) {
        net.tisma.app.p2p.SafeFile.Problem.EMPTY -> R.string.unsafe_empty
        net.tisma.app.p2p.SafeFile.Problem.NOT_A_KIND -> R.string.unsafe_kind
        net.tisma.app.p2p.SafeFile.Problem.PROGRAM -> R.string.unsafe_program
        net.tisma.app.p2p.SafeFile.Problem.DAMAGED -> R.string.unsafe_damaged
        net.tisma.app.p2p.SafeFile.Problem.OUTSIDE_PATH -> R.string.unsafe_path
        net.tisma.app.p2p.SafeFile.Problem.LINK -> R.string.unsafe_link
        net.tisma.app.p2p.SafeFile.Problem.TOO_BIG -> R.string.unsafe_big
    }) + (if (n.detail.isNotBlank()) ": " + n.detail else ""))
    is SliceNotice.ScansArrived -> resources.getQuantityString(R.plurals.scans_arrived, n.count, n.count)
    is SliceNotice.SiteRefused -> when (n.reason) {
        net.tisma.app.engine.SiteLink.Reason.NOT_HTTPS -> getString(R.string.site_not_https)
        net.tisma.app.engine.SiteLink.Reason.SITE -> getString(R.string.site_not_listed)
        net.tisma.app.engine.SiteLink.Reason.KIND -> getString(R.string.site_kind)
        net.tisma.app.engine.SiteLink.Reason.NOT_A_LINK -> getString(R.string.site_bad_link)
        null -> getString(R.string.site_failed, n.detail)
    }
    is SliceNotice.KeptChanges -> getString(R.string.profile_kept_changes,
        n.labels.take(3).joinToString(", ") + if (n.labels.size > 3) " (+${n.labels.size - 3})" else "")
}

private fun displayName(context: Context, uri: Uri): String? = runCatching {
    context.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { c ->
        if (c.moveToFirst()) c.getString(0) else null
    }
}.getOrNull()

/**
 * «Mi biblioteca» on the plate (local_library.md, rules 3 and 7): keep the
 * part with its recipe and its analysis; for a part from the library, what no
 * longer matches.
 */
@Composable
private fun LibraryCard(state: SliceState, slice: SliceViewModel) {
    val lp = state.fromLibrary
    TismaCard(Modifier.testTag("plate_library")) {
        Text(stringResource(R.string.library_title), style = MaterialTheme.typography.titleMedium)
        lp?.let { Text(stringResource(R.string.library_from, it.name), style = MaterialTheme.typography.bodySmall) }
        lp?.check?.let { c ->
            if (c.olderEngine) Text(stringResource(R.string.library_older_engine), color = MaterialTheme.colorScheme.error,
                style = MaterialTheme.typography.bodySmall, modifier = Modifier.testTag("library_older_engine"))
            if (c.missingKeys.isNotEmpty()) Text(stringResource(R.string.library_missing_keys, c.missingKeys.joinToString(", ")),
                style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        if (lp != null && lp.staleAnalyses.isNotEmpty()) {
            Text(stringResource(R.string.library_analysis_stale), color = MaterialTheme.colorScheme.error,
                style = MaterialTheme.typography.bodySmall, modifier = Modifier.testTag("library_analysis_stale"))
            OutlinedButton(onClick = { slice.analyzeModel() }, enabled = !state.analyzing && !state.slicing,
                modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp).testTag("library_reanalyze")) {
                Text(stringResource(R.string.library_reanalyze))
            }
        }
        if (state.result != null || state.analysis != null) {
            Button(onClick = { slice.saveToLibrary() }, modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp).testTag("library_save")) {
                Text(stringResource(R.string.library_save))
            }
            if (lp != null) AccentTextButton(onClick = { slice.saveToLibrary(asNew = true) }, modifier = Modifier.testTag("library_save_new")) {
                Text(stringResource(R.string.library_save_new))
            }
        }
    }
}

/**
 * The Engineering section (owner's decision, 05-11-2026): the plate's part in
 * the 3D view with its analyses (FEA and drag; mobile_app.md, rule 13), and
 * the CAD and the parts.
 */
@Composable
private fun EngineeringControls(
    state: SliceState, slice: SliceViewModel, nav: Navigator,
    pick: androidx.activity.compose.ManagedActivityResultLauncher<Array<String>, Uri?>,
    view: (@Composable (Modifier) -> Unit)?,
) {
    ScreenColumn {
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(onClick = { nav.open(Screen.Cad) }, modifier = Modifier.weight(1f).heightIn(min = 52.dp).testTag("eng_cad")) {
                Icon(painterResource(R.drawable.tisma_design), null); Spacer(Modifier.size(8.dp)); Text(stringResource(R.string.more_cad))
            }
            OutlinedButton(onClick = { nav.open(Screen.Parts) }, modifier = Modifier.weight(1f).heightIn(min = 52.dp).testTag("eng_parts")) {
                Icon(painterResource(R.drawable.tisma_fasteners), null); Spacer(Modifier.size(8.dp)); Text(stringResource(R.string.more_parts))
            }
        }
        view?.invoke(Modifier.fillMaxWidth().height(320.dp))
        TismaCard {
            OutlinedButton(onClick = { pick.launch(arrayOf("*/*")) }, enabled = !state.analyzing,
                modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp).testTag("eng_open")) {
                Icon(painterResource(R.drawable.tisma_folder), null)
                Spacer(Modifier.size(8.dp))
                Text(stringResource(R.string.plate_open))
            }
            Text(state.modelName ?: stringResource(R.string.eng_no_model), style = MaterialTheme.typography.bodyLarge)
        }
        if (!state.engineAvailable) Text(stringResource(R.string.plate_no_engine), color = MaterialTheme.colorScheme.onSurfaceVariant)
        if (state.modelName != null && state.isProject && state.engineAvailable) EngineeringCard(state, slice)
        if (state.modelName != null && !state.isProject && state.engineAvailable && state.mesh != null) ModelEngineeringCard(state, slice)
        if (state.libraryAvailable && state.modelName != null && !state.isProject &&
            (state.analysis != null || state.fromLibrary != null)) LibraryCard(state, slice)
        state.error?.let { e ->
            TismaCard {
                Text(stringResource(R.string.job_error), style = MaterialTheme.typography.titleMedium)
                Text(e.errorEs.takeIf { ctx().isSpanish() && it.isNotBlank() } ?: e.error, color = MaterialTheme.colorScheme.error)
            }
        }
    }
}

/** The print's filament cost: grams × the price per kg, or null without both. */
internal fun plateCost(grams: Double?, pricePerKg: kotlinx.serialization.json.JsonElement?): Double? {
    val price = (pricePerKg as? kotlinx.serialization.json.JsonPrimitive)?.content?.toDoubleOrNull() ?: return null
    if (grams == null || grams <= 0 || price <= 0) return null
    return grams / 1000 * price
}

/** The plates a Bambu Lab's start G-code knows (gcode_macros.md, rule 12); none chosen is «auto». */
val BAMBU_PLATES = listOf("Cool Plate", "Engineering Plate", "High Temp Plate", "Textured PEI Plate", "Supertack Plate", "Textured Cool Plate")

/** A printer of Bambu Lab's (the pack's `Orca_BBL` vendor, or its name). */
internal fun isBambu(p: net.tisma.app.engine.EngineRunner.ProfileChoice?): Boolean =
    p != null && (p.printer.contains("/BBL/") || p.printer.contains("Orca_BBL") || (p.name ?: p.printer).contains("Bambu", ignoreCase = true))
