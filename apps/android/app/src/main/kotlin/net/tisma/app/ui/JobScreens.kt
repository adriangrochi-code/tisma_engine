package net.tisma.app.ui

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
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExtendedFloatingActionButton
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import net.tisma.app.R
import net.tisma.app.cloud.CloudState
import net.tisma.app.cloud.CloudViewModel
import net.tisma.app.cloud.ProjectSource
import net.tisma.cloud.AeroDirection
import net.tisma.cloud.AeroQuality
import net.tisma.cloud.FeaMode
import net.tisma.cloud.FeaQuality
import net.tisma.cloud.Job
import net.tisma.cloud.JobRequest
import net.tisma.cloud.JobStatus
import net.tisma.cloud.allows
import net.tisma.cloud.including
import net.tisma.cloud.summaryLines

/** The list of jobs, newest first (cloud.md, rule 10), followed every 2 s while one runs (rule 8). */
@Composable
fun JobsScreen(state: CloudState, cloud: CloudViewModel, nav: Navigator) {
    if (!state.signedIn) {
        ScreenColumn {
            Text(stringResource(R.string.jobs_sign_in))
            Button(onClick = { nav.open(Screen.Account) }) { Text(stringResource(R.string.sign_in)) }
        }
        return
    }
    // Restarts when a job is added, so a new one is followed too.
    LaunchedEffect(state.jobs.size, state.jobs.count { !it.status.finished }) { cloud.followJobs() }
    Box(Modifier.fillMaxSize()) {
        when {
            !state.jobsLoaded -> CircularProgressIndicator(Modifier.align(Alignment.Center))
            state.jobs.isEmpty() -> Text(stringResource(R.string.jobs_empty), Modifier.align(Alignment.Center),
                color = MaterialTheme.colorScheme.onSurfaceVariant)
            else -> LazyColumn(
                Modifier.fillMaxSize().testTag("jobs"),
                contentPadding = androidx.compose.foundation.layout.PaddingValues(16.dp, 16.dp, 16.dp, 96.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                items(state.jobs, key = { it.id }) { job ->
                    JobCard(job) { nav.open(Screen.JobDetail(job.id)) }
                }
            }
        }
        ExtendedFloatingActionButton(
            onClick = { nav.open(Screen.NewJob) },
            icon = { Icon(painterResource(R.drawable.tisma_slice), null) },
            text = { Text(stringResource(R.string.new_job)) },
            modifier = Modifier.align(Alignment.BottomEnd).padding(16.dp).testTag("new_job"),
        )
    }
}

@Composable
private fun JobCard(job: Job, onClick: () -> Unit) {
    val context = ctx()
    Card(onClick = onClick, modifier = Modifier.fillMaxWidth().testTag("job_" + job.id)) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(context.kindText(job), style = MaterialTheme.typography.titleMedium, modifier = Modifier.weight(1f))
                StatusChip(job.status)
            }
            if (!job.status.finished) {
                LinearProgressIndicator(progress = { job.progress / 100f }, modifier = Modifier.fillMaxWidth())
                Text(stringResource(R.string.job_progress, job.progress), style = MaterialTheme.typography.bodySmall)
            }
            context.jobFailureText(job)?.let {
                Text(it, color = MaterialTheme.colorScheme.error, style = MaterialTheme.typography.bodyMedium,
                    modifier = Modifier.testTag("job_error_" + job.id))
            }
            if (job.createdAt.isNotBlank()) Text(stringResource(R.string.job_created, shortDate(job.createdAt)),
                style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

@Composable
private fun StatusChip(s: JobStatus) {
    val context = ctx()
    val color: Color = when (s) {
        JobStatus.DONE -> Tisma.colors.success
        JobStatus.FAILED -> MaterialTheme.colorScheme.error
        JobStatus.RUNNING, JobStatus.QUEUED -> Tisma.colors.accentText
        else -> MaterialTheme.colorScheme.onSurfaceVariant
    }
    Text(context.statusText(s), color = color, style = MaterialTheme.typography.labelLarge, fontWeight = FontWeight.SemiBold)
}

/** One job: its state, summary or error (rule 19a), and its actions (cancel, save the result). */
@Composable
fun JobDetailScreen(id: String, state: CloudState, cloud: CloudViewModel, nav: Navigator) {
    val context = ctx()
    LaunchedEffect(id) { cloud.followJob(id) }
    val job = state.jobs.firstOrNull { it.id == id }
    val save = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/octet-stream")) { uri ->
        if (uri != null && job != null) {
            cloud.saveResult(job.id, displayName(context, uri) ?: job.resultFilename) {
                context.contentResolver.openOutputStream(uri, "wt")
            }
        }
    }
    ScreenColumn {
        if (job == null) {
            CircularProgressIndicator()
            return@ScreenColumn
        }
        TismaCard {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(context.kindText(job), style = MaterialTheme.typography.titleLarge, modifier = Modifier.weight(1f))
                StatusChip(job.status)
            }
            if (!job.status.finished) {
                LinearProgressIndicator(progress = { job.progress / 100f }, modifier = Modifier.fillMaxWidth())
                Text(stringResource(R.string.job_progress, job.progress))
            }
            if (job.seconds > 0) Text(stringResource(R.string.job_seconds, hours(job.seconds)),
                style = MaterialTheme.typography.bodyMedium)
            if (job.createdAt.isNotBlank()) Text(stringResource(R.string.job_created, shortDate(job.createdAt)),
                style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        context.jobFailureText(job)?.let { text ->
            TismaCard {
                Text(stringResource(R.string.job_error), style = MaterialTheme.typography.titleMedium)
                Text(text, color = MaterialTheme.colorScheme.error, modifier = Modifier.testTag("job_error"))
            }
        }
        val lines = job.summaryLines()
        if (job.status == JobStatus.DONE && lines.isNotEmpty()) {
            TismaCard(Modifier.testTag("summary")) {
                Text(stringResource(R.string.job_summary), style = MaterialTheme.typography.titleMedium)
                context.readableSummary(lines).forEach { (k, v) ->
                    Row {
                        Text(k, Modifier.weight(1f), color = MaterialTheme.colorScheme.onSurfaceVariant)
                        Text(v)
                    }
                }
            }
        }
        if (!job.status.finished) {
            OutlinedButton(onClick = { cloud.cancel(job.id) }, enabled = !state.busy,
                modifier = Modifier.fillMaxWidth().height(52.dp).testTag("cancel_job")) {
                Text(stringResource(R.string.job_cancel))
            }
        }
        if (job.status == JobStatus.DONE && job.kind == "slice") {
            // MA10: the G-code of a cloud slice opens in the preview like a local one (rule 9).
            OutlinedButton(onClick = { cloud.previewResult(job.id) { path -> nav.open(Screen.Preview(path)) } },
                enabled = !state.busy, modifier = Modifier.fillMaxWidth().height(52.dp).testTag("preview_result")) {
                Text(stringResource(R.string.preview_open))
            }
        }
        if (job.status == JobStatus.DONE) {
            Button(onClick = { save.launch(job.resultFilename.ifBlank { "result" }) }, enabled = !state.busy,
                modifier = Modifier.fillMaxWidth().height(56.dp).testTag("save_result")) {
                Icon(painterResource(R.drawable.tisma_save), null)
                Spacer(Modifier.size(8.dp))
                Text(stringResource(R.string.job_save))
            }
        }
    }
}

/** The calculations the cloud runs (rule 6), as the person picks them. */
enum class JobChoice(val label: Int) {
    SLICE(R.string.kind_slice), FEA(R.string.kind_fea), FEA_OPTIMIZE(R.string.kind_fea_optimize),
    AERO(R.string.kind_aero), ORIENT(R.string.kind_orient),
}

/** Sends a 3MF project to slice or analyze (rules 6 and 10b). */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun NewJobScreen(state: CloudState, cloud: CloudViewModel, nav: Navigator) {
    val context = ctx()
    var uriText by rememberSaveable { mutableStateOf<String?>(null) }
    var name by rememberSaveable { mutableStateOf<String?>(null) }
    var choice by rememberSaveable { mutableStateOf(JobChoice.SLICE) }
    var objectIndex by rememberSaveable { mutableStateOf("0") }
    var instance by rememberSaveable { mutableStateOf("0") }
    var feaQuality by rememberSaveable { mutableStateOf(FeaQuality.NORMAL) }
    var aeroQuality by rememberSaveable { mutableStateOf(AeroQuality.FAST) }
    var speed by rememberSaveable { mutableStateOf("10") }
    var direction by rememberSaveable { mutableStateOf(AeroDirection.X) }
    val pick = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) {
            uriText = uri.toString()
            name = displayName(context, uri)
        }
    }
    LaunchedEffect(Unit) { cloud.refreshAccount() }

    val obj = objectIndex.toIntOrNull()
    val inst = instance.toIntOrNull()
    val speedValue = speed.replace(',', '.').toDoubleOrNull()
    val request: JobRequest? = when (choice) {
        JobChoice.SLICE -> JobRequest.Slice
        JobChoice.FEA, JobChoice.FEA_OPTIMIZE -> if (obj != null && inst != null) JobRequest.Fea(
            if (choice == JobChoice.FEA) FeaMode.ANALYZE else FeaMode.LIGHTEST_INFILL, feaQuality, obj, inst) else null
        JobChoice.AERO -> if (obj != null && inst != null && speedValue != null && speedValue > 0 && speedValue <= 100)
            JobRequest.Aero(obj, inst, aeroQuality, speedValue, direction) else null
        JobChoice.ORIENT -> if (obj != null && inst != null) JobRequest.Orient(obj, inst) else null
    }

    ScreenColumn {
        TismaCard {
            OutlinedButton(onClick = { pick.launch(arrayOf("*/*")) }, modifier = Modifier.fillMaxWidth().height(52.dp)
                .testTag("pick_project")) {
                Icon(painterResource(R.drawable.tisma_folder), null)
                Spacer(Modifier.size(8.dp))
                Text(stringResource(R.string.pick_project))
            }
            Text(name ?: stringResource(R.string.project_none), style = MaterialTheme.typography.bodyMedium,
                color = if (name == null) MaterialTheme.colorScheme.onSurfaceVariant else MaterialTheme.colorScheme.onSurface)
        }
        TismaCard {
            Text(stringResource(R.string.what_to_do), style = MaterialTheme.typography.titleMedium)
            FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                JobChoice.entries.forEach { c ->
                    FilterChip(selected = choice == c, onClick = { choice = c },
                        label = { Text(stringResource(c.label)) }, modifier = Modifier.testTag("choice_" + c.name.lowercase()))
                }
            }
            if (choice != JobChoice.SLICE) {
                Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                    NumberField(objectIndex, { objectIndex = it }, R.string.object_index, Modifier.weight(1f))
                    NumberField(instance, { instance = it }, R.string.instance_index, Modifier.weight(1f))
                }
            }
            if (choice == JobChoice.FEA || choice == JobChoice.FEA_OPTIMIZE) {
                Text(stringResource(R.string.fea_quality), style = MaterialTheme.typography.labelLarge)
                FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    listOf(FeaQuality.FAST to R.string.quality_fast, FeaQuality.NORMAL to R.string.quality_normal,
                        FeaQuality.HIGH to R.string.quality_high, FeaQuality.ULTRA to R.string.quality_ultra).forEach { (q, l) ->
                        FilterChip(selected = feaQuality == q, onClick = { feaQuality = q }, label = { Text(stringResource(l)) })
                    }
                }
            }
            if (choice == JobChoice.AERO) {
                Text(stringResource(R.string.fea_quality), style = MaterialTheme.typography.labelLarge)
                FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    listOf(AeroQuality.FRICTION to R.string.quality_friction, AeroQuality.FAST to R.string.quality_fast,
                        AeroQuality.NORMAL to R.string.quality_normal, AeroQuality.HIGH to R.string.quality_high).forEach { (q, l) ->
                        FilterChip(selected = aeroQuality == q, onClick = { aeroQuality = q }, label = { Text(stringResource(l)) })
                    }
                }
                OutlinedTextField(speed, { speed = it }, label = { Text(stringResource(R.string.aero_speed)) },
                    supportingText = { Text(stringResource(R.string.aero_speed_rule)) }, singleLine = true,
                    isError = speedValue == null || speedValue <= 0 || speedValue > 100,
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                    modifier = Modifier.fillMaxWidth().testTag("aero_speed"))
                Text(stringResource(R.string.aero_direction), style = MaterialTheme.typography.labelLarge)
                FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    AeroDirection.entries.forEach { d ->
                        FilterChip(selected = direction == d, onClick = { direction = d }, label = { Text(d.wire) })
                    }
                }
            }
        }
        // Rule 10b: a calculation the plan does not include shows a lock and the plans that have it.
        val plan = state.currentPlan
        if (request != null && plan != null && !plan.allows(request)) {
            val with = state.plans.including(request).joinToString(", ") { it.name }
            TismaCard(Modifier.testTag("locked")) {
                Text("🔒 " + if (with.isEmpty()) stringResource(R.string.locked_no_plan)
                    else stringResource(R.string.locked_plan, with), style = MaterialTheme.typography.bodyLarge)
                AccentTextButton(onClick = { context.openAccountPage(state) }) { Text(stringResource(R.string.see_plans)) }
            }
        }
        Button(
            onClick = {
                val uri = Uri.parse(uriText ?: return@Button)
                val source = ProjectSource(name ?: "project.3mf", sizeOf(context, uri)) {
                    context.contentResolver.openInputStream(uri)
                }
                cloud.submit(request ?: return@Button, source) { id -> nav.replace(Screen.JobDetail(id)) }
            },
            enabled = uriText != null && request != null && !state.uploading && state.signedIn,
            modifier = Modifier.fillMaxWidth().height(56.dp).testTag("send_job"),
        ) {
            Icon(painterResource(R.drawable.tisma_send), null)
            Spacer(Modifier.size(8.dp))
            Text(stringResource(if (state.uploading) R.string.sending else R.string.send_job))
        }
        if (state.uploading) LinearProgressIndicator(Modifier.fillMaxWidth())
    }
}

@Composable
private fun NumberField(value: String, onChange: (String) -> Unit, label: Int, modifier: Modifier) {
    OutlinedTextField(value, { v -> onChange(v.filter(Char::isDigit).take(4)) }, label = { Text(stringResource(label)) },
        singleLine = true, isError = value.toIntOrNull() == null,
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number), modifier = modifier)
}

private fun displayName(context: android.content.Context, uri: Uri): String? = runCatching {
    context.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { c ->
        if (c.moveToFirst()) c.getString(0) else null
    }
}.getOrNull()

private fun sizeOf(context: android.content.Context, uri: Uri): Long = runCatching {
    context.contentResolver.query(uri, arrayOf(OpenableColumns.SIZE), null, null, null)?.use { c ->
        if (c.moveToFirst() && !c.isNull(0)) c.getLong(0) else -1L
    }
}.getOrNull() ?: -1L
