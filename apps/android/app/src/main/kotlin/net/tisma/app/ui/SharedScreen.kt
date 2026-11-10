package net.tisma.app.ui

import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.os.BatteryManager
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
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
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.pluralStringResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import net.tisma.app.R
import net.tisma.app.p2p.SharedViewModel
import net.tisma.app.p2p.SharingPolicy
import java.time.ZoneId
import kotlin.time.toJavaInstant
import java.time.format.DateTimeFormatter

/**
 * The shared library (p2p_library.md) on the phone: the block list and what it withdrew (rule 4), whether this
 * phone shares and why not (rules 3 and 6), the upload limit, and what other peers see (rule 8). Searching and
 * downloading wait for the server's index.
 */
@Composable
fun SharedScreen(shared: SharedViewModel) {
    val state by shared.state.collectAsStateWithLifecycle()
    val context = LocalContext.current
    LaunchedEffect(Unit) {
        shared.refresh()
        shared.setConditions(unmetered(context), charging(context))
    }
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp).testTag("shared"),
        verticalArrangement = Arrangement.spacedBy(12.dp)) {
        // Rule 4: a withdrawn copy is deleted, and said.
        if (state.withdrawn.isNotEmpty()) TismaCard(Modifier.testTag("shared_withdrawn")) {
            Text(pluralStringResource(R.plurals.shared_withdrawn, state.withdrawn.size, state.withdrawn.size), color = MaterialTheme.colorScheme.error)
            AccentTextButton(onClick = { shared.withdrawnSeen() }) { Text(stringResource(R.string.shared_ok)) }
        }
        // Rule 7: a report sent, or why not.
        state.reported?.let { TismaCard(Modifier.testTag("shared_reported")) {
            Text(stringResource(R.string.shared_reported)); AccentTextButton(onClick = { shared.reportSeen() }) { Text(stringResource(R.string.shared_ok)) } } }
        state.reportFailed?.let { TismaCard(Modifier.testTag("shared_report_failed")) {
            Text(stringResource(R.string.shared_report_failed, it), color = MaterialTheme.colorScheme.error)
            AccentTextButton(onClick = { shared.reportSeen() }) { Text(stringResource(R.string.shared_ok)) } } }
        TismaCard {
            Text(stringResource(R.string.shared_index), style = MaterialTheme.typography.titleMedium)
            if (state.index == null) Text(stringResource(R.string.shared_no_index), style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.testTag("shared_index_soon"))
            else {
                OutlinedTextField(state.query, { shared.setQuery(it) }, singleLine = true, label = { Text(stringResource(R.string.profile_search)) },
                    modifier = Modifier.fillMaxWidth().testTag("shared_search"))
                Text(stringResource(R.string.shared_download_soon), style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
        state.found.take(200).forEach { m -> ModelCard(m) { reason -> shared.report(m.id, null, reason) } }
        TismaCard(Modifier.testTag("shared_list")) {
            Text(stringResource(R.string.shared_blocklist), style = MaterialTheme.typography.titleMedium)
            val list = state.list
            Text(if (list == null) stringResource(R.string.shared_no_list)
                else stringResource(R.string.shared_list_date,
                    DateTimeFormatter.ofPattern("dd-MM-yyyy HH:mm").withZone(ZoneId.systemDefault()).format(list.generated.toJavaInstant())),
                style = MaterialTheme.typography.bodySmall)
            // The detail (in English, from the check) goes to the log; the person reads their own language.
            if (state.problem != null) Text(stringResource(R.string.shared_problem), style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.error, modifier = Modifier.testTag("shared_problem"))
        }
        TismaCard(Modifier.testTag("shared_sharing")) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(stringResource(R.string.shared_share), modifier = Modifier.weight(1f))
                Switch(state.sharing, { shared.setSharing(it) },
                    modifier = Modifier.testTag("shared_share_switch").named(stringResource(R.string.shared_share)))
            }
            // Rule 8: said before it is turned on.
            Text(stringResource(R.string.shared_privacy), style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant)
            Text(stringResource(when (state.blockedBy) {
                null -> R.string.shared_now_sharing
                SharingPolicy.Why.OFF -> R.string.shared_off
                SharingPolicy.Why.NO_LIST -> R.string.shared_why_no_list
                SharingPolicy.Why.STALE_LIST -> R.string.shared_why_stale
                SharingPolicy.Why.NOT_WIFI -> R.string.shared_why_wifi
                SharingPolicy.Why.NOT_CHARGING -> R.string.shared_why_charging
            }), style = MaterialTheme.typography.bodySmall, modifier = Modifier.testTag("shared_why"),
                color = if (state.blockedBy == null) MaterialTheme.colorScheme.onSurface else MaterialTheme.colorScheme.onSurfaceVariant)
            var cap by rememberSaveable { mutableStateOf(state.uploadCapMb.toString()) }
            OutlinedTextField(cap, { t -> cap = t.filter { it.isDigit() }.take(6); cap.toIntOrNull()?.let { shared.setUploadCap(it) } },
                singleLine = true, label = { Text(stringResource(R.string.shared_upload_cap)) },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                modifier = Modifier.fillMaxWidth().testTag("shared_upload_cap"))
        }
    }
}

/** Wi-Fi or another network that is not metered (rule 3, P2P6: not on mobile data). */
private fun unmetered(context: Context): Boolean = runCatching {
    val cm = context.getSystemService(ConnectivityManager::class.java)
    val caps = cm.getNetworkCapabilities(cm.activeNetwork) ?: return false
    caps.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_METERED)
}.getOrDefault(false)

private fun charging(context: Context): Boolean = runCatching {
    val battery = context.registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED)) ?: return false
    val status = battery.getIntExtra(BatteryManager.EXTRA_STATUS, -1)
    status == BatteryManager.BATTERY_STATUS_CHARGING || status == BatteryManager.BATTERY_STATUS_FULL
}.getOrDefault(false)

/** A model of the index: who made it, its licence, whether a seeder checked it, and «Denunciar» (rule 7). */
@Composable
private fun ModelCard(m: net.tisma.app.p2p.LibraryIndex.Model, report: (String) -> Unit) {
    var asking by rememberSaveable(m.id) { mutableStateOf(false) }
    TismaCard(Modifier.testTag("shared_model_" + m.id)) {
        Text(m.name, style = MaterialTheme.typography.titleMedium)
        Text(listOf(m.author, m.license, sizeText(m.size)).filter { it.isNotBlank() }.joinToString(" · "), style = MaterialTheme.typography.bodySmall)
        if (m.checked) Text(stringResource(R.string.shared_checked), style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant)
        if (m.description.isNotBlank()) Text(m.description, style = MaterialTheme.typography.bodySmall, maxLines = 3)
        if (!asking) AccentTextButton(onClick = { asking = true }, modifier = Modifier.testTag("shared_report_" + m.id)) {
            Text(stringResource(R.string.shared_report))
        } else {
            Text(stringResource(R.string.shared_report_why), style = MaterialTheme.typography.labelLarge)
            listOf(R.string.report_rights, R.string.report_dangerous, R.string.report_unsafe, R.string.report_other).forEach { r ->
                val text = stringResource(r)
                AccentTextButton(onClick = { report(text); asking = false }, modifier = Modifier.testTag("shared_report_reason_$r")) { Text(text) }
            }
        }
    }
}

private fun sizeText(b: Long) = if (b <= 0) "" else if (b >= 1L shl 20) "%.1f MB".format(b / 1048576.0) else "${(b + 1023) / 1024} KB"
