package net.tisma.app.ui

import android.os.Build
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.Checkbox
import androidx.compose.material3.FilterChip
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
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import net.tisma.app.BuildConfig
import net.tisma.app.R
import net.tisma.app.cloud.CloudState
import net.tisma.app.cloud.CloudViewModel
import net.tisma.app.data.AppLog
import net.tisma.app.data.AppSettings
import net.tisma.cloud.ProblemReport

/** What a report attaches when the person accepts it (problem_reports.md, rule 3). */
fun attachment(): Triple<String, String, String> {
    val version = BuildConfig.VERSION_NAME
    val os = "Android ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT}), ${Build.MANUFACTURER} ${Build.MODEL}".take(80)
    // No printer or profiles in the app yet; they join the header when they exist.
    val log = "Tisma for Android $version\n$os\n\n" + AppLog.text()
    return Triple(version, os, log)
}

/** Report a problem (cloud.md, rule 11; problem_reports.md, rules 1 to 9). Nothing is sent without «Send». */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun ReportScreen(state: CloudState, cloud: CloudViewModel, nav: Navigator) {
    var kind by rememberSaveable { mutableStateOf("bug") }
    var title by rememberSaveable { mutableStateOf("") }
    var description by rememberSaveable { mutableStateOf("") }
    var steps by rememberSaveable { mutableStateOf("") }
    var expected by rememberSaveable { mutableStateOf("") }
    var attach by rememberSaveable { mutableStateOf(true) }
    var showAttached by rememberSaveable { mutableStateOf(false) }
    var tooShort by rememberSaveable { mutableStateOf(false) }
    ScreenColumn {
        Text(stringResource(R.string.report_intro), style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(stringResource(R.string.report_privacy), style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant)
        TismaCard {
            FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                listOf("bug" to R.string.report_kind_bug, "crash" to R.string.report_kind_crash,
                    "suggestion" to R.string.report_kind_suggestion).forEach { (k, l) ->
                    FilterChip(selected = kind == k, onClick = { kind = k }, label = { Text(stringResource(l)) },
                        modifier = Modifier.testTag("report_kind_$k"))
                }
            }
            OutlinedTextField(title, { title = it.take(200); tooShort = false }, singleLine = true,
                label = { Text(stringResource(R.string.report_words)) },
                placeholder = { Text(stringResource(R.string.report_words_hint)) },
                isError = tooShort,
                supportingText = if (tooShort) { { Text(stringResource(R.string.report_too_short)) } } else null,
                modifier = Modifier.fillMaxWidth().testTag("report_words"))
            OutlinedTextField(description, { description = it.take(10_000) },
                label = { Text(stringResource(R.string.report_what)) }, minLines = 3,
                modifier = Modifier.fillMaxWidth().testTag("report_what"))
            OutlinedTextField(steps, { steps = it.take(10_000) },
                label = { Text(stringResource(R.string.report_steps)) }, minLines = 2, modifier = Modifier.fillMaxWidth())
            OutlinedTextField(expected, { expected = it.take(10_000) }, singleLine = true,
                label = { Text(stringResource(R.string.report_expected)) }, modifier = Modifier.fillMaxWidth())
            Row(verticalAlignment = Alignment.CenterVertically) {
                Checkbox(attach, { attach = it }, modifier = Modifier.testTag("report_attach"))
                Text(stringResource(R.string.report_attach), style = MaterialTheme.typography.bodyMedium)
            }
            AccentTextButton(onClick = { showAttached = true }) { Text(stringResource(R.string.report_see_attached)) }
        }
        Text(if (state.signedIn) stringResource(R.string.report_with_account, state.email.orEmpty())
            else stringResource(R.string.report_without_account), style = MaterialTheme.typography.bodySmall)
        Button(
            onClick = {
                if (title.trim().length < 3) {
                    tooShort = true
                    return@Button
                }
                val (version, os, log) = attachment()
                cloud.sendReport(ProblemReport(
                    kind = kind, title = title.trim(), description = description, steps = steps, expected = expected,
                    appVersion = if (attach) version else null, os = if (attach) os else null,
                    log = if (attach) log else null,
                )) { nav.back() }
            },
            enabled = !state.busy,
            modifier = Modifier.fillMaxWidth().height(56.dp).testTag("report_send"),
        ) { Text(stringResource(R.string.send)) }
        if (state.busy) LinearProgressIndicator(Modifier.fillMaxWidth())
    }
    if (showAttached) {
        AlertDialog(
            onDismissRequest = { showAttached = false },
            confirmButton = { AccentTextButton(onClick = { showAttached = false }) { Text(stringResource(R.string.ok)) } },
            title = { Text(stringResource(R.string.report_see_attached)) },
            text = {
                Column(Modifier.heightIn(max = 400.dp).verticalScroll(rememberScrollState())) {
                    Text(attachment().third, fontFamily = FontFamily.Monospace, style = MaterialTheme.typography.bodySmall)
                }
            },
        )
    }
}

/** The account's own reports, newest first (problem_reports.md, rule 11). */
@Composable
fun MyReportsScreen(state: CloudState, cloud: CloudViewModel) {
    val context = ctx()
    LaunchedEffect(Unit) { cloud.loadMyReports() }
    ScreenColumn {
        if (state.reports.isEmpty()) Text(stringResource(R.string.my_reports_empty),
            color = MaterialTheme.colorScheme.onSurfaceVariant)
        state.reports.forEach { r ->
            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(16.dp)) {
                    Text(r.title, style = MaterialTheme.typography.titleMedium)
                    Text(shortDate(r.createdAt) + " · " + context.reportStatusText(r.status),
                        style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            }
        }
    }
}

@Composable
fun PreferencesScreen(state: CloudState, cloud: CloudViewModel, settings: AppSettings) {
    var server by rememberSaveable { mutableStateOf(state.server) }
    var prefer by rememberSaveable { mutableStateOf(settings.cloudPrefer) }
    ScreenColumn {
        TismaCard {
            OutlinedTextField(server, { server = it }, singleLine = true,
                label = { Text(stringResource(R.string.pref_server)) },
                placeholder = { Text(stringResource(R.string.pref_server_hint)) },
                modifier = Modifier.fillMaxWidth().testTag("pref_server"))
            OutlinedButton(onClick = { cloud.setServer(server); server = net.tisma.cloud.CloudClient.normalizeServer(server) },
                modifier = Modifier.testTag("pref_save")) { Text(stringResource(R.string.pref_save)) }
        }
        TismaCard {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) {
                    Text(stringResource(R.string.pref_cloud_prefer), style = MaterialTheme.typography.bodyLarge)
                    Text(stringResource(R.string.pref_cloud_prefer_hint), style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
                Switch(prefer, { prefer = it; cloud.setCloudPrefer(it) })
            }
        }
        var search by rememberSaveable { mutableStateOf(settings.searchNetwork) }
        TismaCard {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(stringResource(R.string.pref_search_network), modifier = Modifier.weight(1f))
                Switch(search, { search = it; settings.searchNetwork = it }, modifier = Modifier.testTag("pref_search"))
            }
        }
        Text(stringResource(R.string.pref_version, BuildConfig.VERSION_NAME), style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant)
    }
}
