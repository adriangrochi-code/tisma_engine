package net.tisma.app.ui

import androidx.annotation.StringRes
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Card
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import net.tisma.app.R
import net.tisma.app.engine.Knowledge
import net.tisma.app.engine.SettingsLayout

/** One entry of the help, whatever its kind, to list and search them together. */
private data class Entry(val kind: String, val id: String, val title: String, val text: String)

/**
 * The offline help (mobile_app.md, rule 16): problems, procedures, glossary,
 * materials and calibration guides from the profile pack, searched together.
 * A cause's «setting» action applies its value to the slice; a «procedure» or
 * «problem» action opens that entry. The sources are named at the end.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun HelpScreen(k: Knowledge?, apply: (String, kotlinx.serialization.json.JsonElement) -> Unit, label: (String) -> String) {
    if (k == null || k.empty) {
        ScreenColumn { Text(stringResource(R.string.help_none), color = MaterialTheme.colorScheme.onSurfaceVariant) }
        return
    }
    var query by rememberSaveable { mutableStateOf("") }
    var kind by rememberSaveable { mutableStateOf("problems") }
    var open by rememberSaveable { mutableStateOf<String?>(null) }
    val all = k.problems.map { Entry("problems", it.id, it.name, it.symptoms + " " + it.causes.joinToString(" ") { c -> c.name }) } +
        k.procedures.map { Entry("procedures", it.id, it.name, it.forWhat) } +
        k.glossary.map { Entry("glossary", it.term, it.term, it.definition) } +
        k.materials.map { Entry("materials", it.name, it.name, "") } +
        k.calibrations.map { Entry("calibrations", it.id, it.name, it.why) }
    val words = net.tisma.app.engine.searchWords(query)
    val shown = if (words.isNotEmpty()) all.filter { e -> val t = net.tisma.app.engine.fold(e.title + " " + e.text); words.all { w -> t.contains(w) } }
        else all.filter { it.kind == kind }
    fun go(toKind: String, id: String) { kind = toKind; query = ""; open = "$toKind/$id" }
    LazyColumn(Modifier.fillMaxSize().testTag("help"), contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        item {
            OutlinedTextField(query, { query = it }, singleLine = true, label = { Text(stringResource(R.string.help_search)) },
                modifier = Modifier.fillMaxWidth().testTag("help_search"))
        }
        if (words.isEmpty()) item {
            FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                listOf("problems" to R.string.help_problems, "procedures" to R.string.help_procedures, "glossary" to R.string.help_glossary,
                    "materials" to R.string.help_materials, "calibrations" to R.string.help_calibrations).forEach { (id, l) ->
                    FilterChip(selected = kind == id, onClick = { kind = id }, label = { Text(stringResource(l)) },
                        modifier = Modifier.testTag("help_kind_$id"))
                }
            }
        }
        items(shown, key = { it.kind + "/" + it.id }) { e ->
            val key = e.kind + "/" + e.id
            Card(Modifier.fillMaxWidth().clickable { open = if (open == key) null else key }.testTag("help_" + key)) {
                Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    Text(e.title, style = MaterialTheme.typography.titleMedium)
                    if (open != key) {
                        if (e.text.isNotBlank()) Text(e.text, style = MaterialTheme.typography.bodySmall, maxLines = 2,
                            color = MaterialTheme.colorScheme.onSurfaceVariant)
                    } else Details(k, e, apply, label, ::go)
                }
            }
        }
        val src = k.sources[if (words.isEmpty()) kind else "problems"].orEmpty()
        if (src.isNotEmpty()) item {
            Text(stringResource(R.string.help_sources, src.joinToString("; ")), style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

@Composable
private fun Details(k: Knowledge, e: Entry, apply: (String, kotlinx.serialization.json.JsonElement) -> Unit, label: (String) -> String,
                    go: (String, String) -> Unit) {
    @Composable fun line(@StringRes title: Int, text: String) {
        if (text.isBlank()) return
        Text(stringResource(title), style = MaterialTheme.typography.labelLarge)
        Text(text, style = MaterialTheme.typography.bodyMedium)
    }
    when (e.kind) {
        "problems" -> k.problems.firstOrNull { it.id == e.id }?.let { p ->
            line(R.string.help_symptoms, p.symptoms)
            p.causes.forEachIndexed { i, c ->
                Text("${i + 1}. ${c.name}", style = MaterialTheme.typography.titleSmall, modifier = Modifier.padding(top = 6.dp))
                if (c.explain.isNotBlank()) Text(c.explain, style = MaterialTheme.typography.bodyMedium)
                if (c.check.isNotBlank()) Text(c.check, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                c.actions.forEach { a ->
                    when (a.type) {
                        "setting" -> a.value?.let { v ->
                            OutlinedButton(onClick = { apply(a.key, v) }, modifier = Modifier.testTag("help_apply_${a.key}")) {
                                Text(stringResource(R.string.help_apply, label(a.key), SettingsLayout.show(v)))
                            }
                            if (a.why.isNotBlank()) Text(a.why, style = MaterialTheme.typography.bodySmall)
                        }
                        "procedure" -> k.procedures.firstOrNull { it.id == a.id }?.let { pr ->
                            OutlinedButton(onClick = { go("procedures", pr.id) }) { Text(pr.name) }
                        }
                        "problem" -> k.problems.firstOrNull { it.id == a.id }?.let { pb ->
                            OutlinedButton(onClick = { go("problems", pb.id) }) { Text(pb.name) }
                        }
                    }
                }
            }
        }
        "procedures" -> k.procedures.firstOrNull { it.id == e.id }?.let { p ->
            line(R.string.help_for, p.forWhat)
            p.steps.forEachIndexed { i, s ->
                Text("${i + 1}. ${s.title}", style = MaterialTheme.typography.titleSmall, modifier = Modifier.padding(top = 6.dp))
                Text(s.text, style = MaterialTheme.typography.bodyMedium)
            }
        }
        "glossary" -> k.glossary.firstOrNull { it.term == e.id }?.let { Text(it.definition, style = MaterialTheme.typography.bodyMedium) }
        "materials" -> k.materials.firstOrNull { it.name == e.id }?.let { m ->
            m.lines.forEach { (key, v) ->
                val name = when (key) {
                    "nozzle_temp" -> R.string.help_nozzle_temp
                    "bed_temp" -> R.string.help_bed_temp
                    "fan" -> R.string.help_fan
                    "bed_required" -> R.string.help_bed_required
                    "enclosure" -> R.string.help_enclosure
                    "nozzle_material" -> R.string.help_nozzle_material
                    else -> R.string.help_surfaces
                }
                line(name, v)
            }
        }
        "calibrations" -> k.calibrations.firstOrNull { it.id == e.id }?.let { c ->
            line(R.string.help_why, c.why)
            c.steps.forEachIndexed { i, s -> Text("${i + 1}. $s", style = MaterialTheme.typography.bodyMedium) }
            line(R.string.help_result, c.result)
        }
    }
}
