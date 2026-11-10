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
import androidx.compose.foundation.layout.heightIn
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
fun PrintersScreen(state: PrintersState, printers: PrintersViewModel, nav: Navigator, search: Boolean = true,
                   header: @Composable () -> Unit = {}) {
    WhileVisible(state.printers.size) { printers.followAll() }
    val context = ctx()
    // The print notices need this permission from Android 13 on; asked once there are printers.
    val askNotify = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { }
    LaunchedEffect(state.printers.isNotEmpty()) {
        if (state.printers.isNotEmpty() && android.os.Build.VERSION.SDK_INT >= 33 &&
            androidx.core.content.ContextCompat.checkSelfPermission(context, android.Manifest.permission.POST_NOTIFICATIONS) !=
            android.content.pm.PackageManager.PERMISSION_GRANTED) {
            askNotify.launch(android.Manifest.permission.POST_NOTIFICATIONS)
        }
    }
    if (search) {
        // Multicast discovery keeps the Wi-Fi awake: only while the list is on screen, not in the background.
        WhileVisible {
            val d = net.tisma.app.printers.NetworkDiscovery(context, printers::onFound, printers::onLost)
            d.start()
            try { kotlinx.coroutines.awaitCancellation() } finally { d.stop() }
        }
        // A printer not heard for 3 minutes leaves the list.
        WhileVisible {
            while (true) {
                kotlinx.coroutines.delay(10_000)
                printers.refreshFound()
            }
        }
    }
    Box(Modifier.fillMaxSize()) {
        ScreenColumn {
            header()
            if (state.printers.isEmpty()) {
                Text(stringResource(R.string.printers_empty), color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            state.printers.forEach { p ->
                PrinterCard(p, state.status[p.id]) { nav.open(Screen.PrinterDetail(p.id)) }
            }
            if (state.found.isNotEmpty()) {
                Text(stringResource(R.string.printers_found), style = MaterialTheme.typography.titleMedium,
                    modifier = Modifier.padding(top = 8.dp))
                state.found.forEach { f ->
                    Card(onClick = { nav.open(Screen.AddPrinter(null, f)) }, modifier = Modifier.fillMaxWidth().testTag("found_" + f.key)) {
                        Column(Modifier.padding(16.dp)) {
                            Text(f.name, style = MaterialTheme.typography.titleMedium)
                            Text(kindName(f.kind) + " · " + f.url + (if (f.model.isNotBlank()) " · " + f.model else ""),
                                style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                        }
                    }
                }
            } else if (search && state.printers.isEmpty()) {
                Text(stringResource(R.string.printers_searching), style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            Spacer(Modifier.height(72.dp))
        }
        ExtendedFloatingActionButton(
            onClick = { nav.open(Screen.AddPrinter(null)) },
            icon = { Icon(painterResource(R.drawable.tisma_printer), null) },
            text = { Text(stringResource(R.string.printer_add)) },
            modifier = Modifier.align(Alignment.BottomEnd).padding(16.dp).testTag("add_printer").named(stringResource(R.string.printer_add)),
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
    WhileVisible(id) { printers.follow(id) }
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
                    modifier = Modifier.weight(1f).heightIn(min = 56.dp).testTag("printer_pause")) { Text(stringResource(R.string.printer_pause)) }
                OutlinedButton(onClick = { confirmCancel = true }, enabled = !state.busy,
                    modifier = Modifier.weight(1f).heightIn(min = 56.dp).testTag("printer_cancel")) { Text(stringResource(R.string.printer_cancel)) }
            }
            PrinterState.PAUSED -> Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                Button(onClick = { printers.resume(id) }, enabled = !state.busy,
                    modifier = Modifier.weight(1f).heightIn(min = 56.dp).testTag("printer_resume")) { Text(stringResource(R.string.printer_resume)) }
                OutlinedButton(onClick = { confirmCancel = true }, enabled = !state.busy,
                    modifier = Modifier.weight(1f).heightIn(min = 56.dp).testTag("printer_cancel")) { Text(stringResource(R.string.printer_cancel)) }
            }
            else -> Unit
        }
        if (p.kind != PrinterKind.PRUSALINK) ControlsCard(id, s, state, printers)
        TismaCard {
            Text(stringResource(R.string.printer_files), style = MaterialTheme.typography.titleMedium)
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(stringResource(R.string.printer_start_after), modifier = Modifier.weight(1f))
                Switch(startAfterUpload, { startAfterUpload = it },
                    modifier = Modifier.testTag("printer_start_after").named(stringResource(R.string.printer_start_after)))
            }
            Button(onClick = { pick.launch(arrayOf("*/*")) },
                enabled = !state.uploading && s?.state != PrinterState.PRINTING && s?.state != PrinterState.PAUSED,
                modifier = Modifier.fillMaxWidth().heightIn(min = 56.dp).testTag("printer_upload")) {
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
    WhileVisible(id) { printers.followCamera(id) }
    val view = state.cameras[id] ?: return
    TismaCard(Modifier.testTag("camera")) {
        when (view) {
            CameraView.Connecting -> Text(stringResource(R.string.camera_connecting), color = MaterialTheme.colorScheme.onSurfaceVariant)
            CameraView.None -> Text(stringResource(R.string.camera_none), color = MaterialTheme.colorScheme.onSurfaceVariant)
            is CameraView.Failed -> Text(stringResource(R.string.camera_failed, view.reason), color = MaterialTheme.colorScheme.error)
            is CameraView.BambuFailed -> Text(stringResource(when (view.kind) {
                net.tisma.printers.PrinterException.Kind.NETWORK -> R.string.camera_bambu_network
                net.tisma.printers.PrinterException.Kind.AUTH -> R.string.camera_bambu_auth
                net.tisma.printers.PrinterException.Kind.CERTIFICATE -> R.string.camera_bambu_certificate
                else -> R.string.camera_bambu_no_images
            }), color = MaterialTheme.colorScheme.error, modifier = Modifier.testTag("camera_bambu_failed"))
            is CameraView.Image -> {
                val bitmap = androidx.compose.runtime.remember(view) {
                    android.graphics.BitmapFactory.decodeByteArray(view.jpeg, 0, view.jpeg.size)
                        ?.let { cameraTurned(it, view.flipHorizontal, view.flipVertical, view.rotation) }?.asImageBitmap()
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

/**
 * Temperatures (any time), and homing, moving and extruding when the printer is idle
 * (docs/MOBILE.md, «Controles»). Numbers are typed with the numeric keyboard (mobile_app.md, rule 17).
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun ControlsCard(id: String, s: PrinterStatus?, state: PrintersState, printers: PrintersViewModel) {
    if (s == null || s.state == PrinterState.OFFLINE || s.state == PrinterState.ERROR) return
    val idle = s.state == PrinterState.READY
    var nozzle by rememberSaveable { mutableStateOf("") }
    var bed by rememberSaveable { mutableStateOf("") }
    var step by rememberSaveable { mutableStateOf(10.0) }
    TismaCard(Modifier.testTag("controls")) {
        Text(stringResource(R.string.controls_title), style = MaterialTheme.typography.titleMedium)
        listOf(Triple(net.tisma.printers.Heater.NOZZLE, R.string.controls_nozzle, nozzle),
            Triple(net.tisma.printers.Heater.BED, R.string.controls_bed, bed)).forEach { (heater, label, value) ->
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(value, { v -> val t = v.filter { it.isDigit() }.take(3)
                        if (heater == net.tisma.printers.Heater.NOZZLE) nozzle = t else bed = t },
                    singleLine = true, label = { Text(stringResource(label)) },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                    modifier = Modifier.weight(1f).testTag("temp_" + heater.name.lowercase()))
                OutlinedButton(onClick = { value.toDoubleOrNull()?.let { printers.setTemperature(id, heater, it) } },
                    enabled = !state.busy && value.toDoubleOrNull() != null,
                    modifier = Modifier.testTag("temp_set_" + heater.name.lowercase())) { Text(stringResource(R.string.controls_set)) }
                AccentTextButton(onClick = { printers.setTemperature(id, heater, 0.0) }, enabled = !state.busy) {
                    Text(stringResource(R.string.controls_off))
                }
            }
        }
        if (idle) {
            Text(stringResource(R.string.controls_move), style = MaterialTheme.typography.labelLarge)
            FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                listOf(0.1, 1.0, 10.0).forEach { d ->
                    FilterChip(selected = step == d, onClick = { step = d },
                        label = { Text(String.format(Locale.getDefault(), "%s mm", if (d < 1) "0.1" else d.toInt().toString())) })
                }
            }
            net.tisma.printers.Axis.entries.forEach { axis ->
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
                    Text(axis.name, modifier = Modifier.weight(0.3f), style = MaterialTheme.typography.titleMedium)
                    OutlinedButton(onClick = { printers.jog(id, axis, -step) }, enabled = !state.busy,
                        modifier = Modifier.weight(1f).testTag("jog_minus_" + axis.name.lowercase())) { Text("−") }
                    OutlinedButton(onClick = { printers.jog(id, axis, step) }, enabled = !state.busy,
                        modifier = Modifier.weight(1f).testTag("jog_plus_" + axis.name.lowercase())) { Text("+") }
                }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedButton(onClick = { printers.home(id) }, enabled = !state.busy,
                    modifier = Modifier.weight(1f).testTag("home")) { Text(stringResource(R.string.controls_home)) }
                OutlinedButton(onClick = { printers.extrude(id, 5.0) }, enabled = !state.busy,
                    modifier = Modifier.weight(1f).testTag("extrude")) { Text(stringResource(R.string.controls_extrude)) }
                OutlinedButton(onClick = { printers.extrude(id, -5.0) }, enabled = !state.busy,
                    modifier = Modifier.weight(1f)) { Text(stringResource(R.string.controls_retract)) }
            }
        }
    }
}

/** Adds or edits a printer: its kind, name, address and key; it is saved only when the test passes. */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun AddPrinterScreen(editId: String?, state: PrintersState, printers: PrintersViewModel, nav: Navigator,
                     found: net.tisma.printers.FoundPrinter? = null) {
    val existing = state.printers.firstOrNull { it.id == editId }
    var kind by rememberSaveable { mutableStateOf(existing?.kind ?: found?.kind ?: PrinterKind.MOONRAKER) }
    var name by rememberSaveable { mutableStateOf(existing?.name ?: found?.name.orEmpty()) }
    var address by rememberSaveable { mutableStateOf(existing?.url ?: found?.url.orEmpty()) }
    var key by rememberSaveable { mutableStateOf("") }
    var user by rememberSaveable { mutableStateOf(existing?.user?.ifBlank { null } ?: "maker") }
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
            if (kind == PrinterKind.PRUSALINK) {
                OutlinedTextField(user, { user = it }, singleLine = true, label = { Text(stringResource(R.string.printer_user)) },
                    modifier = Modifier.fillMaxWidth().testTag("printer_user"))
            }
            OutlinedTextField(key, { key = it }, singleLine = true,
                label = { Text(stringResource(when (kind) {
                    PrinterKind.OCTOPRINT -> R.string.printer_key
                    PrinterKind.MOONRAKER -> R.string.printer_key_optional
                    PrinterKind.PRUSALINK -> R.string.printer_password
                    PrinterKind.DUET -> R.string.printer_board_password
                    PrinterKind.BAMBU -> R.string.printer_access_code
                })) },
                supportingText = if (existing != null) { { Text(stringResource(R.string.printer_key_keep)) } } else null,
                visualTransformation = PasswordVisualTransformation(),
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password),
                modifier = Modifier.fillMaxWidth().testTag("printer_key"))
            Button(
                onClick = {
                    // An empty key on an edit keeps the stored one.
                    val secret = key.ifEmpty { if (existing != null) null else "" }
                    printers.testAndSave(existing?.id, name, kind, address, secret,
                        if (kind == PrinterKind.PRUSALINK) user else "") { nav.back() }
                    key = ""
                },
                enabled = !state.busy && address.isNotBlank() && (kind !in setOf(PrinterKind.OCTOPRINT, PrinterKind.PRUSALINK, PrinterKind.BAMBU) || key.isNotBlank() || existing != null),
                modifier = Modifier.fillMaxWidth().heightIn(min = 56.dp).testTag("printer_save"),
            ) { Text(stringResource(R.string.printer_test_save)) }
            if (state.busy) LinearProgressIndicator(Modifier.fillMaxWidth())
            if (kind == PrinterKind.BAMBU) Text(stringResource(R.string.printer_bambu_hint), style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant)
            Text(stringResource(R.string.printer_key_safe), style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
    state.confirm?.let { c ->
        AlertDialog(
            onDismissRequest = { printers.rejectCertificate() },
            title = { Text(stringResource(R.string.bambu_confirm_title)) },
            text = { Text(stringResource(R.string.bambu_confirm_text, c.serial, c.fingerprint), modifier = Modifier.testTag("bambu_confirm_text")) },
            confirmButton = {
                AccentTextButton(onClick = { printers.confirmCertificate { nav.back() } }, modifier = Modifier.testTag("bambu_confirm")) {
                    Text(stringResource(R.string.bambu_confirm_yes))
                }
            },
            dismissButton = { AccentTextButton(onClick = { printers.rejectCertificate() }) { Text(stringResource(R.string.cancel)) } },
        )
    }
}

// ---- Words ----

@Composable
private fun kindName(k: PrinterKind) = stringResource(when (k) {
    PrinterKind.MOONRAKER -> R.string.kind_moonraker
    PrinterKind.OCTOPRINT -> R.string.kind_octoprint
    PrinterKind.PRUSALINK -> R.string.kind_prusalink
    PrinterKind.DUET -> R.string.kind_duet
    PrinterKind.BAMBU -> R.string.kind_bambu
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
        PrinterException.Kind.UNSUPPORTED -> getString(R.string.printer_error_unsupported)
        PrinterException.Kind.CERTIFICATE -> getString(R.string.printer_error_certificate)
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

/**
 * The camera image as the printer's settings say it is mounted (printer_connections.md, «Cámara de la
 * impresora»: flips, then a clockwise turn), so a camera upside down is seen the right way.
 */
internal fun cameraTurned(b: android.graphics.Bitmap, flipH: Boolean, flipV: Boolean, rotation: Int): android.graphics.Bitmap {
    if (!flipH && !flipV && rotation % 360 == 0) return b
    val m = android.graphics.Matrix().apply {
        postScale(if (flipH) -1f else 1f, if (flipV) -1f else 1f, b.width / 2f, b.height / 2f)
        postRotate(rotation.toFloat())
    }
    return android.graphics.Bitmap.createBitmap(b, 0, 0, b.width, b.height, m, true)
}
