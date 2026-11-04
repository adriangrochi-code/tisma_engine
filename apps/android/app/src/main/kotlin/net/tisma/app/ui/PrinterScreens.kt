package net.tisma.app.ui

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
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
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.ExtendedFloatingActionButton
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import net.tisma.app.R
import net.tisma.app.printers.CameraView
import net.tisma.app.printers.PrinterNotice
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.foundation.layout.aspectRatio
import net.tisma.app.printers.PrintersState
import net.tisma.app.printers.PrintersViewModel
import net.tisma.printers.PrinterConfig
import net.tisma.printers.PrinterException
import net.tisma.printers.PrinterKind
import net.tisma.printers.PrinterState
import net.tisma.printers.PrinterStatus
import net.tisma.printers.Temperature
import java.util.Locale

/** The printers on the network and their state (MOBILE.md, «Impresoras»). */
@Composable
fun PrintersScreen(state: PrintersState, printers: PrintersViewModel, nav: Navigator) {
    LaunchedEffect(state.printers.size) { printers.followAll() }
    Box(Modifier.fillMaxSize()) {
        ScreenColumn {
            if (state.printers.isEmpty()) {
                Text(stringResource(R.string.printers_empty), color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            state.printers.forEach { p ->
                PrinterCard(p, state.status[p.id]) { nav.open(Screen.PrinterDetail(p.id)) }
            }
            Spacer(Modifier.height(72.dp))
        }
        ExtendedFloatingActionButton(
            onClick = { nav.open(Screen.AddPrinter(null)) },
            icon = { Icon(painterResource(R.drawable.tisma_printer), null) },
            text = { Text(stringResource(R.string.printer_add)) },
            modifier = Modifier.align(Alignment.BottomEnd).padding(16.dp).testTag("add_printer"),
        )
    }
}

@Composable
private fun PrinterCard(p: PrinterConfig, s: PrinterStatus?, onClick: () -> Unit) {
    Card(onClick = onClick, modifier = Modifier.fillMaxWidth().testTag("printer_" + p.id)) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(p.name, style = MaterialTheme.typography.titleMedium, modifier = Modifier.weight(1f))
                StateChip(s?.state)
            }
            Text(kindName(p.kind) + " · " + p.url, style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant)
            s?.percent?.let { pct ->
                LinearProgressIndicator(progress = { (pct / 100).toFloat() }, modifier = Modifier.fillMaxWidth())
                Text(s.file + " · " + percentText(pct), style = MaterialTheme.typography.bodyMedium)
            }
            s?.let { TemperaturesLine(it) }
        }
    }
}

@Composable
private fun StateChip(s: PrinterState?) {
    val (text, color) = when (s) {
        null -> R.string.printer_state_unknown to MaterialTheme.colorScheme.onSurfaceVariant
        PrinterState.OFFLINE -> R.string.printer_state_offline to MaterialTheme.colorScheme.onSurfaceVariant
        PrinterState.READY -> R.string.printer_state_ready to Tisma.colors.success
        PrinterState.PRINTING -> R.string.printer_state_printing to Tisma.colors.accentText
        PrinterState.PAUSED -> R.string.printer_state_paused to Tisma.colors.accentText
        PrinterState.ERROR -> R.string.printer_state_error to MaterialTheme.colorScheme.error
        PrinterState.BUSY -> R.string.printer_state_busy to MaterialTheme.colorScheme.onSurfaceVariant
    }
    Text(stringResource(text), color = color, style = MaterialTheme.typography.labelLarge, fontWeight = FontWeight.SemiBold,
        modifier = Modifier.testTag("printer_state"))
}

@Composable
private fun TemperaturesLine(s: PrinterStatus) {
    val parts = listOfNotNull(
        s.nozzle?.let { stringResource(R.string.temp_nozzle, temp(it)) },
        s.bed?.let { stringResource(R.string.temp_bed, temp(it)) },
        s.chamber?.let { stringResource(R.string.temp_chamber, temp(it)) },
    )
    if (parts.isNotEmpty()) Text(parts.joinToString("   "), style = MaterialTheme.typography.bodyMedium,
        modifier = Modifier.testTag("temperatures"))
}

/** One printer: what it prints, temperatures, and its controls (MOBILE.md, «Impresión», «Controles», «Archivos»). */
@Composable
fun PrinterDetailScreen(id: String, state: PrintersState, printers: PrintersViewModel, nav: Navigator) {
    val context = ctx()
    val p = state.printers.firstOrNull { it.id == id } ?: return
    val s = state.status[id]
    var confirmCancel by rememberSaveable { mutableStateOf(false) }
    var confirmRemove by rememberSaveable { mutableStateOf(false) }
    var startAfterUpload by rememberSaveable { mutableStateOf(true) }
    LaunchedEffect(id) { printers.follow(id) }
    val pick = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        if (uri != null) {
            val (name, size) = nameAndSize(context, uri)
            printers.upload(id, name ?: "print.gcode", size, startAfterUpload) { context.contentResolver.openInputStream(uri) }
        }
    }
    ScreenColumn {
        TismaCard {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(p.name, style = MaterialTheme.typography.titleLarge, modifier = Modifier.weight(1f))
                StateChip(s?.state)
            }
            if (s == null) LinearProgressIndicator(Modifier.fillMaxWidth())
            s?.message?.takeIf { it.isNotBlank() }?.let {
                Text(it, color = if (s.state == PrinterState.ERROR) MaterialTheme.colorScheme.error
                    else MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.testTag("printer_message"))
            }
            val pct = s?.percent
            if (s != null && pct != null) {
                Text(percentText(pct), style = MaterialTheme.typography.displaySmall, color = Tisma.colors.accentText,
                    modifier = Modifier.testTag("printer_percent"))
                LinearProgressIndicator(progress = { (pct / 100).toFloat() }, modifier = Modifier.fillMaxWidth())
                if (s.file.isNotBlank()) Text(s.file, style = MaterialTheme.typography.bodyLarge)
                val layer = s.layer
                val total = s.totalLayers
                if (layer != null && total != null && total > 0) {
                    Text(stringResource(R.string.printer_layer, layer, total))
                }
                s.elapsedS?.let { Text(stringResource(R.string.printer_elapsed, duration(it))) }
                s.remainingS?.let { Text(stringResource(R.string.printer_remaining, duration(it))) }
            }
            s?.let { TemperaturesLine(it) }
        }
        CameraBox(id, state, printers)
        when (s?.state) {
            PrinterState.PRINTING -> Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                Button(onClick = { printers.pause(id) }, enabled = !state.busy,
                    modifier = Modifier.weight(1f).height(56.dp).testTag("printer_pause")) { Text(stringResource(R.string.printer_pause)) }
                OutlinedButton(onClick = { confirmCancel = true }, enabled = !state.busy,
                    modifier = Modifier.weight(1f).height(56.dp).testTag("printer_cancel")) { Text(stringResource(R.string.printer_cancel)) }
            }
            PrinterState.PAUSED -> Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                Button(onClick = { printers.resume(id) }, enabled = !state.busy,
                    modifier = Modifier.weight(1f).height(56.dp).testTag("printer_resume")) { Text(stringResource(R.string.printer_resume)) }
                OutlinedButton(onClick = { confirmCancel = true }, enabled = !state.busy,
                    modifier = Modifier.weight(1f).height(56.dp).testTag("printer_cancel")) { Text(stringResource(R.string.printer_cancel)) }
            }
            else -> Unit
        }
        TismaCard {
            Text(stringResource(R.string.printer_files), style = MaterialTheme.typography.titleMedium)
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(stringResource(R.string.printer_start_after), modifier = Modifier.weight(1f))
                Switch(startAfterUpload, { startAfterUpload = it }, modifier = Modifier.testTag("printer_start_after"))
            }
            Button(onClick = { pick.launch(arrayOf("*/*")) },
                enabled = !state.uploading && s?.state != PrinterState.PRINTING && s?.state != PrinterState.PAUSED,
                modifier = Modifier.fillMaxWidth().height(56.dp).testTag("printer_upload")) {
                Icon(painterResource(R.drawable.tisma_send), null)
                Spacer(Modifier.size(8.dp))
                Text(stringResource(if (state.uploading) R.string.sending else R.string.printer_upload))
            }
            if (state.uploading) LinearProgressIndicator(Modifier.fillMaxWidth())
        }
        Row {
            AccentTextButton(onClick = { nav.open(Screen.AddPrinter(id)) }) { Text(stringResource(R.string.printer_edit)) }
            AccentTextButton(onClick = { confirmRemove = true }) { Text(stringResource(R.string.printer_remove)) }
        }
    }
    if (confirmCancel) {
        AlertDialog(
            onDismissRequest = { confirmCancel = false },
            title = { Text(stringResource(R.string.printer_cancel_title)) },
            text = { Text(stringResource(R.string.printer_cancel_text)) },
            confirmButton = {
                AccentTextButton(onClick = { confirmCancel = false; printers.cancel(id) },
                    modifier = Modifier.testTag("printer_cancel_confirm")) { Text(stringResource(R.string.printer_cancel)) }
            },
            dismissButton = { AccentTextButton(onClick = { confirmCancel = false }) { Text(stringResource(R.string.back)) } },
        )
    }
    if (confirmRemove) {
        AlertDialog(
            onDismissRequest = { confirmRemove = false },
            title = { Text(stringResource(R.string.printer_remove)) },
            text = { Text(p.name) },
            confirmButton = {
                AccentTextButton(onClick = { confirmRemove = false; printers.remove(id); nav.back() }) {
                    Text(stringResource(R.string.printer_remove))
                }
            },
            dismissButton = { AccentTextButton(onClick = { confirmRemove = false }) { Text(stringResource(R.string.back)) } },
        )
    }
}

@Composable
private fun CameraBox(id: String, state: PrintersState, printers: PrintersViewModel) {
    LaunchedEffect(id) { printers.followCamera(id) }
    val view = state.cameras[id] ?: return
    TismaCard(Modifier.testTag("camera")) {
        when (view) {
            CameraView.Connecting -> Text(stringResource(R.string.camera_connecting), color = MaterialTheme.colorScheme.onSurfaceVariant)
            CameraView.None -> Text(stringResource(R.string.camera_none), color = MaterialTheme.colorScheme.onSurfaceVariant)
            is CameraView.Failed -> Text(stringResource(R.string.camera_failed, view.reason), color = MaterialTheme.colorScheme.error)
            is CameraView.Image -> {
                val bitmap = androidx.compose.runtime.remember(view) {
                    android.graphics.BitmapFactory.decodeByteArray(view.jpeg, 0, view.jpeg.size)?.asImageBitmap()
                }
                if (bitmap != null) {
                    androidx.compose.foundation.Image(bitmap, stringResource(R.string.camera_title),
                        modifier = Modifier.fillMaxWidth().aspectRatio(bitmap.width.toFloat() / bitmap.height.coerceAtLeast(1))
                            .testTag("camera_image"))
                } else {
                    Text(stringResource(R.string.camera_failed, "JPEG"), color = MaterialTheme.colorScheme.error)
                }
            }
        }
    }
}

/** Adds or edits a printer: its kind, name, address and key; it is saved only when the test passes. */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun AddPrinterScreen(editId: String?, state: PrintersState, printers: PrintersViewModel, nav: Navigator) {
    val existing = state.printers.firstOrNull { it.id == editId }
    var kind by rememberSaveable { mutableStateOf(existing?.kind ?: PrinterKind.MOONRAKER) }
    var name by rememberSaveable { mutableStateOf(existing?.name.orEmpty()) }
    var address by rememberSaveable { mutableStateOf(existing?.url.orEmpty()) }
    var key by rememberSaveable { mutableStateOf("") }
    ScreenColumn {
        TismaCard {
            FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                PrinterKind.entries.forEach { k ->
                    FilterChip(selected = kind == k, onClick = { kind = k }, label = { Text(kindName(k)) },
                        modifier = Modifier.testTag("kind_" + k.wire))
                }
            }
            OutlinedTextField(name, { name = it }, singleLine = true, label = { Text(stringResource(R.string.printer_name)) },
                modifier = Modifier.fillMaxWidth().testTag("printer_name"))
            OutlinedTextField(address, { address = it }, singleLine = true,
                label = { Text(stringResource(R.string.printer_address)) },
                placeholder = { Text(if (kind == PrinterKind.MOONRAKER) "192.168.1.20:7125" else "192.168.1.30") },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Uri),
                modifier = Modifier.fillMaxWidth().testTag("printer_address"))
            OutlinedTextField(key, { key = it }, singleLine = true,
                label = { Text(stringResource(if (kind == PrinterKind.OCTOPRINT) R.string.printer_key else R.string.printer_key_optional)) },
                supportingText = if (existing != null) { { Text(stringResource(R.string.printer_key_keep)) } } else null,
                visualTransformation = PasswordVisualTransformation(),
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password),
                modifier = Modifier.fillMaxWidth().testTag("printer_key"))
            Button(
                onClick = {
                    // An empty key on an edit keeps the stored one.
                    val secret = key.ifEmpty { if (existing != null) null else "" }
                    printers.testAndSave(existing?.id, name, kind, address, secret) { nav.back() }
                    key = ""
                },
                enabled = !state.busy && address.isNotBlank() && (kind != PrinterKind.OCTOPRINT || key.isNotBlank() || existing != null),
                modifier = Modifier.fillMaxWidth().height(56.dp).testTag("printer_save"),
            ) { Text(stringResource(R.string.printer_test_save)) }
            if (state.busy) LinearProgressIndicator(Modifier.fillMaxWidth())
            Text(stringResource(R.string.printer_key_safe), style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

// ---- Words ----

@Composable
private fun kindName(k: PrinterKind) = stringResource(when (k) {
    PrinterKind.MOONRAKER -> R.string.kind_moonraker
    PrinterKind.OCTOPRINT -> R.string.kind_octoprint
})

private fun temp(t: Temperature): String =
    if (t.target > 0) String.format(Locale.getDefault(), "%.0f / %.0f °C", t.actual, t.target)
    else String.format(Locale.getDefault(), "%.0f °C", t.actual)

private fun percentText(p: Double) = String.format(Locale.getDefault(), "%.0f %%", p)

/** A time as «1 h 02 min» or «12 min». */
fun duration(seconds: Long): String {
    val h = seconds / 3600
    val m = (seconds % 3600) / 60
    return if (h > 0) String.format(Locale.ROOT, "%d h %02d min", h, m) else "$m min"
}

fun Context.printerNoticeText(n: PrinterNotice): String = when (n) {
    is PrinterNotice.Failed -> when (n.error.kind) {
        PrinterException.Kind.NETWORK -> getString(R.string.printer_error_network)
        PrinterException.Kind.AUTH -> getString(R.string.printer_error_auth, n.error.detail)
        PrinterException.Kind.CONFLICT -> getString(R.string.printer_error_conflict, n.error.detail)
        PrinterException.Kind.UNEXPECTED -> getString(R.string.printer_error_unexpected)
        PrinterException.Kind.SERVER -> getString(R.string.printer_error_server, n.error.detail)
    }
    is PrinterNotice.Tested -> getString(R.string.printer_tested, n.text)
    is PrinterNotice.Uploaded -> getString(if (n.started) R.string.printer_uploaded_started else R.string.printer_uploaded, n.name)
    PrinterNotice.CannotRead -> getString(R.string.error_cannot_read)
}

private fun nameAndSize(context: Context, uri: Uri): Pair<String?, Long> = runCatching {
    context.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE), null, null, null)?.use { c ->
        if (c.moveToFirst()) c.getString(0) to (if (c.isNull(1)) -1L else c.getLong(1)) else null
    }
}.getOrNull() ?: (null to -1L)
