package net.tisma.app.ui

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
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
    ScreenColumn {
        PrinterCard(state) { nav.open(Screen.ProfilePicker) }
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
                        enabled = !state.slicing)
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
                            FilterChip(selected = sendTo == p.id, onClick = { sendTo = p.id }, label = { Text(p.name) })
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
